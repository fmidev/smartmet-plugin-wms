#include "WMSGetMap.h"
#include "CaseInsensitiveComparator.h"
#include "Colour.h"
#include "Mime.h"
#include "TemplateFactory.h"
#include "WMS.h"
#include "WMSException.h"
#include <boost/algorithm/string/erase.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
void check_getmap_request_options(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    // check that all mandatory options has been defined
    std::string missing_options;

    if (!theHTTPRequest.getParameter("VERSION"))
    {
      throw Spine::Exception(BCP, "Version not defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    if (!theHTTPRequest.getParameter("LAYERS"))
    {
      throw Spine::Exception(BCP, "At least one layer must be defined in GetMap request")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
    }

    if (!theHTTPRequest.getParameter("STYLES"))
    {
      throw Spine::Exception(BCP, "STYLES-option must be defined, even if it is empty")
          .addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED);
    }

    if (!theHTTPRequest.getParameter("CRS"))
    {
      throw Spine::Exception(BCP, "CRS-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    if (!theHTTPRequest.getParameter("BBOX"))
    {
      throw Spine::Exception(BCP, "BBOX-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE);
    }

    if (!theHTTPRequest.getParameter("WIDTH"))
    {
      throw Spine::Exception(BCP, "WIDTH-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE);
    }

    if (!theHTTPRequest.getParameter("HEIGHT"))
    {
      throw Spine::Exception(BCP, "HEIGHT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE);
    }

    if (!theHTTPRequest.getParameter("FORMAT"))
    {
      throw Spine::Exception(BCP, "FORMAT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "GetMap request options did not validate!");
  }
}

void validate_options(const tag_get_map_request_options& options,
                      const WMSConfig& itsConfig,
                      const Engine::Querydata::Engine& querydata)
{
  try
  {
    // check the requested version number
    if (!itsConfig.isValidVersion(options.version))
    {
      throw Spine::Exception(BCP, "The requested version is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED)
          .addParameter("Requested version", options.version)
          .addParameter("Supported versions",
                        boost::algorithm::join(itsConfig.supportedWMSVersions(), "','"));
    }

    // check whether the requested layers, styles, CRS are valid
    for (unsigned int i = 0; i < options.map_info_vector.size(); i++)
    {
      std::string layer(options.map_info_vector[i].name);
      std::string style(options.map_info_vector[i].style);

      // check that layer is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidLayer(layer))
      {
        throw Spine::Exception(BCP, "The requested layer is not supported!")
            .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
            .addParameter("Requested layer", layer);
      }

      // check that style is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidStyle(layer, style))
      {
        throw Spine::Exception(BCP, "The style is not supported by the requested layer!")
            .addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED)
            .addParameter("Requested style", style)
            .addParameter("Requested layer", layer);
      }

      // check that CRS is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidCRS(layer, options.bbox.crs))
      {
        throw Spine::Exception(BCP, "The CRS is not supported by the requested layer!")
            .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_CRS)
            .addParameter("Requested CRS", options.bbox.crs)
            .addParameter("Requested layer", layer);
      }

      // check that given timesteps are valid
      for (const boost::posix_time::ptime& timestamp : options.timesteps)
      {
        if (!itsConfig.isValidTime(layer, timestamp, querydata))
        {
          throw Spine::Exception(BCP, "Invalid time requested!")
              .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
              .addParameter("Requested time", Fmi::to_iso_string(timestamp))
              .addParameter("Requested layer", layer);
        }
      }
    }

    // check format
    if (!itsConfig.isValidMapFormat(options.format))
    {
      throw Spine::Exception(BCP, "The requested map format is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_FORMAT)
          .addParameter("Requested map format", options.format)
          .addParameter("Supported map formats",
                        boost::algorithm::join(itsConfig.supportedMapFormats(), "','"));
    }

    // check bbox
    if (options.bbox.xMin > options.bbox.xMax || options.bbox.yMin > options.bbox.yMax)
    {
      throw Spine::Exception(BCP, "Invalid BBOX definition!")
          .addDetail("'xMin' must be smaller than 'xMax' and 'yMin' must be smaller than 'yMax'.")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
          .addParameter("xMin", std::to_string(options.bbox.xMin))
          .addParameter("yMin", std::to_string(options.bbox.yMin))
          .addParameter("xMax", std::to_string(options.bbox.xMax))
          .addParameter("yMax", std::to_string(options.bbox.yMax));
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "GetMap options did not validate!");
  }
}

// if iso-string contains fractions of seconds remove them
// so that Fmi::TimeParser knows how to parse
boost::posix_time::ptime parse_time(const std::string& time)
{
  try
  {
    std::string t(time);
    // remove second fractions
    if (t.find("Z") != std::string::npos && t.find(".") != std::string::npos &&
        t.find("Z") > t.find("."))  // iso string with fractions of second
    {
      size_t len(t.find("Z") - t.find("."));
      t.erase(t.find("."), len);
    }

    return Fmi::TimeParser::parse(t);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Parsing GetMap time failed!");
  }
}

void parse_interval_with_resolution(const std::string time_str,
                                    std::vector<boost::posix_time::ptime>& timesteps)
{
  try
  {
    // interval defined by its lower bound, upper bound and resolution: min/max/res
    std::vector<std::string> parts;
    boost::algorithm::split(parts, time_str, boost::algorithm::is_any_of("/"));

    if (parts.size() != 3)
    {
      Spine::Exception exception(BCP, "Invalid time interval!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      exception.addParameter("Time interval", time_str);
      throw exception;
    }

    CaseInsensitiveComparator cicomp;
    boost::posix_time::ptime startTime =
        (cicomp(parts[0], "current") ? boost::posix_time::second_clock::universal_time()
                                     : parse_time(parts[0]));
    boost::posix_time::ptime endTime =
        (cicomp(parts[1], "current") ? boost::posix_time::second_clock::universal_time()
                                     : parse_time(parts[1]));

    if (endTime < startTime)
    {
      Spine::Exception exception(BCP, "Invalid time interval!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      exception.addParameter("Time interval", time_str);
      throw exception;
    }

    std::string resolution(parts[2]);

    unsigned int total_minutes = resolution_in_minutes(resolution);

    if (total_minutes > 0)
    {
      while (startTime <= endTime)
      {
        timesteps.push_back(startTime);
        startTime += boost::posix_time::minutes(total_minutes);
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Parsing GetMap time interval failed!");
  }
}

}  // anonymous namespace

WMSGetMap::WMSGetMap(const WMSConfig& theConfig) : itsConfig(theConfig) {}
void WMSGetMap::parseHTTPRequest(const Engine::Querydata::Engine& theQEngine,
                                 Spine::HTTP::Request& theRequest)
{
  try
  {
    itsParameters.debug = Spine::optional_bool(theRequest.getParameter("debug"), false);

    // check all mandatory options
    check_getmap_request_options(theRequest);

    // parse the request
    std::vector<std::string> styles;
    std::vector<std::string> layers;
    std::string layers_opt = *(theRequest.getParameter("LAYERS"));
    std::string styles_opt = *(theRequest.getParameter("STYLES"));
    if (!layers_opt.empty())
      boost::algorithm::split(layers, layers_opt, boost::algorithm::is_any_of(","));

    for (unsigned int i = 0; i < layers.size(); i++)
    {
      if (!itsConfig.isValidLayer(layers[i]))
      {
        Spine::Exception exception(BCP, "The requested layer is not supported!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
        exception.addParameter("Layer", layers[i]);
        throw exception;
      }
    }

    if (!styles_opt.empty())
      boost::algorithm::split(styles, styles_opt, boost::algorithm::is_any_of(","));

    // if empty STYLES definition, fill the vector with default style names
    if (styles_opt.size() == 0)
      styles.insert(styles.begin(), layers.size(), "");

    // there must be one-to-one relationship between layers and styles
    // so if styles has been defined there must be equal amount of layers
    if (styles.size() != layers.size())
    {
      Spine::Exception exception(BCP, "LAYERS and STYLES amount mismatch");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      exception.addParameter("Layers", std::to_string(layers.size()));
      exception.addParameter("Styles", std::to_string(styles.size()));
      throw exception;
    }

    // width and height
    try
    {
      itsParameters.width = boost::lexical_cast<unsigned int>(*theRequest.getParameter("width"));
      itsParameters.height = boost::lexical_cast<unsigned int>(*theRequest.getParameter("height"));
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Invalid width / height value!")
          .addDetail("The WIDTH and HEIGHT options must be valid integer numbers!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    for (unsigned int i = 0; i < layers.size(); i++)
    {
      std::string layerName(layers[i]);

      if (!itsConfig.isValidLayer(layerName))
      {
        Spine::Exception exception(BCP, "The requested layer is not supported!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
        exception.addParameter("Requested layer", layerName);
        throw exception;
      }
      std::string layerCustomer(itsConfig.layerCustomer(layerName));
      std::string layerStyle(styles[i]);

      itsParameters.map_info_vector.push_back(tag_map_info(layerName, layerStyle));
    }

    std::string crs = *(theRequest.getParameter("CRS"));  // desired CRS name

    std::string bbox = *(theRequest.getParameter("BBOX"));

    itsParameters.bbox = Spine::BoundingBox(bbox + "," + crs);

    itsParameters.format = *(theRequest.getParameter("FORMAT"));

    CaseInsensitiveComparator cicomp;

    std::string transparent_str(
        Spine::optional_string(theRequest.getParameter("TRANSPARENT"), "FALSE"));
    if (false == cicomp(transparent_str, "true") && false == cicomp(transparent_str, "false"))
    {
      Spine::Exception exception(BCP, "Invalid value for the TRANSPARENT option!");
      exception.addDetail("The TRANSPARENT option must have value 'TRUE' or 'FALSE'");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      throw exception;
    }

    itsParameters.transparent = transparent_str == "TRUE";

    // store  BGCOLOR option
    std::string bgcolor_str = Spine::optional_string(theRequest.getParameter("BGCOLOR"), "");
    if (!bgcolor_str.empty())
    {
      rgb_color bgcol = hex_string_to_rgb(bgcolor_str);
      itsParameters.bgcolor.red = bgcol.red;
      itsParameters.bgcolor.green = bgcol.green;
      itsParameters.bgcolor.blue = bgcol.blue;
    }
    else
    {
      itsParameters.bgcolor.red = 0xFF;
      itsParameters.bgcolor.green = 0xFF;
      itsParameters.bgcolor.blue = 0xFF;
    }

    itsParameters.version = *(theRequest.getParameter("VERSION"));

    if (theRequest.getParameter("TIME"))
    {
      std::string time_str = Spine::optional_string(theRequest.getParameter("TIME"), "current");

      if (true == cicomp(time_str, "current"))
      {
        // current time: send the most current data available,
        // for example current icemap can be up to 24 hours old, but it is still the most current
        // current time for each layer is resolved just before request is passed to dali
      }
      else if (time_str.find(',') == std::string::npos && time_str.find('/') == std::string::npos)
      {
        // single time: time1
        itsParameters.timesteps.push_back(parse_time(time_str));
      }
      else if (time_str.find(',') != std::string::npos && time_str.find('/') == std::string::npos)
      {
        // time list separated with comma: time1,time2,time3
        std::vector<std::string> timesteps;
        boost::algorithm::split(timesteps, time_str, boost::algorithm::is_any_of(","));

        for (unsigned int i = 0; i < timesteps.size(); i++)
          itsParameters.timesteps.push_back(cicomp(timesteps[i], "current")
                                                ? boost::posix_time::second_clock::universal_time()
                                                : parse_time(timesteps[i]));
      }
      else if (time_str.find(',') == std::string::npos && time_str.find('/') != std::string::npos)
      {
        // one interval: min1/max1/res1
        parse_interval_with_resolution(time_str, itsParameters.timesteps);
      }
      else if (time_str.find(',') != std::string::npos && time_str.find('/') != std::string::npos)
      {
        // multiple intervals: min1/max1/res1,min2/max2/res2,...
        std::vector<std::string> parts;
        boost::algorithm::split(parts, time_str, boost::algorithm::is_any_of(","));

        for (unsigned int i = 0; i < parts.size(); i++)
          parse_interval_with_resolution(parts[i], itsParameters.timesteps);
      }
      else
      {
        Spine::Exception exception(BCP, "Invalid TIME option value!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
        exception.addParameter("Time value", time_str);
        throw exception;
      }
    }
    else
    {
      // if time not defined use current time
      // current time for each layer is resolved just before request is passed to dali
    }

    // validate the given options
    validate_options(itsParameters, itsConfig, theQEngine);

    std::string layerName(layers.size() > 0 ? layers.back() : "");

    // Valite authorization for the layers

    // Convert format to image type
    theRequest.removeParameter("format");
    theRequest.addParameter("type", Dali::demimetype(itsParameters.format));

    // Convert WMS width & height to projection xsize & ysize
    theRequest.removeParameter("width");
    theRequest.removeParameter("height");
    theRequest.addParameter("projection.bbox", bbox);
    theRequest.addParameter("projection.xsize", Fmi::to_string(itsParameters.width));
    theRequest.addParameter("projection.ysize", Fmi::to_string(itsParameters.height));

    // This must be done after the validate_options call or we will not get the
    // correct exception as output

    std::string crs_decl = itsConfig.getCRSDefinition(crs);  // Pass GDAL string to renderer
    theRequest.addParameter("projection.crs", crs_decl);

    // Bounding box should always be defined using the main crs

    theRequest.addParameter("customer", itsConfig.layerCustomer(layerName));

    // resolve current time (most recent) for the layer
    std::string time_str = Spine::optional_string(theRequest.getParameter("TIME"), "current");
    if (true == cicomp(time_str, "current"))
    {
      if (!itsConfig.isTemporal(layerName))
      {
        theRequest.removeParameter("time");
      }
      else
      {
        if (false == itsConfig.currentValue(layerName))
        {
          Spine::Exception exception(BCP, "Invalid TIME option value for the current layer!");
          exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
          exception.addParameter("Time value", time_str);
          exception.addParameter("Layer", layerName);
          throw exception;
        }

        boost::posix_time::ptime mostCurrentTime(itsConfig.mostCurrentTime(layerName));
        if (mostCurrentTime.is_not_a_date_time())
          theRequest.removeParameter("time");
        else
          theRequest.addParameter("time", Fmi::to_iso_string(mostCurrentTime));
      }
    }
    else
    {
      if (itsParameters.timesteps.empty())
        throw Spine::Exception(BCP, "Intervals need to be at least one minute long");
      theRequest.addParameter("time", Fmi::to_iso_string(itsParameters.timesteps[0]));
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to complete GetMap request!");
  }
}

Json::Value WMSGetMap::json() const
{
  try
  {
    const tag_map_info map_info = itsParameters.map_info_vector.back();

    return itsConfig.json(map_info.name);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Extracting GetMap JSON failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
