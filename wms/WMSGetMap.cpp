#include "WMS.h"
#include "WMSException.h"
#include "WMSGetMap.h"

#include "TemplateFactory.h"

#include <macgyver/StringConversion.h>

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

#include <spine/Exception.h>
#include <spine/Convenience.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
struct CaseInsensitiveComparator : std::binary_function<std::string, std::string, bool>
{
  char asciilower(char ch) const
  {
    char ret = ch;
    if (ch >= 'A' && ch <= 'Z')
      ret = static_cast<char>(ch + ('a' - 'A'));
    return ret;
  }

  bool operator()(const std::string& first, const std::string& second) const
  {
    std::size_t n = std::min(first.size(), second.size());
    for (std::size_t i = 0; i < n; i++)
    {
      char ch1 = asciilower(first[i]);
      char ch2 = asciilower(second[i]);
      if (ch1 != ch2)
        return false;
    }

    return (first.size() == second.size());
  }
};

std::string demimetype(const std::string& theMimeType)
{
  if (theMimeType == "image/png")
    return "png";
  if (theMimeType == "application/pdf")
    return "pdf";
  if (theMimeType == "application/postscript")
    return "ps";
  if (theMimeType == "image/svg+xml")
    return "svg";

  throw Spine::Exception(BCP, "Unknown mime type requested: '" + theMimeType + "'");
}

void check_getmap_request_options(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    // check that all mandatory options has been defined
    std::string missing_options;

    if (!theHTTPRequest.getParameter("VERSION"))
    {
      Spine::Exception exception(BCP, "Version not defined");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      throw exception;
    }

    if (!theHTTPRequest.getParameter("LAYERS"))
    {
      Spine::Exception exception(BCP, "At least one layer must be defined in GetMap request");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
      throw exception;
    }

    if (!theHTTPRequest.getParameter("STYLES"))
    {
      Spine::Exception exception(BCP, "STYLES-option must be defined, even if it is empty");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED);
      throw exception;
    }

    if (!theHTTPRequest.getParameter("CRS"))
    {
      Spine::Exception exception(BCP, "CRS-option has not been defined");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      throw exception;
    }

    if (!theHTTPRequest.getParameter("BBOX"))
    {
      Spine::Exception exception(BCP, "BBOX-option has not been defined");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE);
      throw exception;
    }

    if (!theHTTPRequest.getParameter("WIDTH"))
    {
      Spine::Exception exception(BCP, "WIDTH-option has not been defined");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE);
      throw exception;
    }

    if (!theHTTPRequest.getParameter("HEIGHT"))
    {
      Spine::Exception exception(BCP, "HEIGHT-option has not been defined");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE);
      throw exception;
    }

    if (!theHTTPRequest.getParameter("FORMAT"))
    {
      Spine::Exception exception(BCP, "FORMAT-option has not been defined");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      throw exception;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      Spine::Exception exception(BCP, "The requested version is not supported!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED);
      exception.addParameter("Requested version", options.version);
      exception.addParameter("Supported versions",
                             boost::algorithm::join(itsConfig.supportedWMSVersions(), "','"));
      throw exception;
    }

    // check whether the requested layers, styles, CRS are valid
    for (unsigned int i = 0; i < options.map_info_vector.size(); i++)
    {
      std::string layer(options.map_info_vector[i].name);
      std::string style(options.map_info_vector[i].style);

      // check that layer is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidLayer(layer))
      {
        Spine::Exception exception(BCP, "The requested layer is not supported!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
        exception.addParameter("Requested layer", layer);
        throw exception;
      }

      // check that style is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidStyle(layer, style))
      {
        Spine::Exception exception(BCP,
                                   "The requested style is not supported by the current layer!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED);
        exception.addParameter("Requested style", style);
        exception.addParameter("Requested layer", layer);
        throw exception;
      }

      // check that CRS is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidCRS(layer, options.bbox.crs))
      {
        Spine::Exception exception(BCP, "The requested CRS is not supported by the current layer!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_CRS);
        exception.addParameter("Requested CRS", options.bbox.crs);
        exception.addParameter("Requested layer", layer);
        throw exception;
      }

      // check that given timesteps are valid
      BOOST_FOREACH (const boost::posix_time::ptime& timestamp, options.timesteps)
      {
        if (!itsConfig.isValidTime(layer, timestamp, querydata))
        {
          Spine::Exception exception(BCP, "Invalid time requested!");
          exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
          exception.addParameter("Requested time", Fmi::to_iso_string(timestamp));
          exception.addParameter("Requested layer", layer);
          throw exception;
        }
      }
    }

    // check format
    if (!itsConfig.isValidMapFormat(options.format))
    {
      Spine::Exception exception(BCP, "The requested map format is not supported!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_FORMAT);
      exception.addParameter("Requested map format", options.format);
      exception.addParameter("Supported map formats",
                             boost::algorithm::join(itsConfig.supportedMapFormats(), "','"));
      throw exception;
    }

    // check bbox
    if (options.bbox.xMin > options.bbox.xMax || options.bbox.yMin > options.bbox.yMax)
    {
      Spine::Exception exception(BCP, "Invalid BBOX definition!");
      exception.addDetail(
          "'xMin' must be smaller than 'xMax' and 'yMin' must be smaller than 'yMax'.");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      exception.addParameter("xMin", std::to_string(options.bbox.xMin));
      exception.addParameter("yMin", std::to_string(options.bbox.yMin));
      exception.addParameter("xMax", std::to_string(options.bbox.xMax));
      exception.addParameter("yMax", std::to_string(options.bbox.yMax));
      throw exception;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // anonymous namespace

WMSGetMap::WMSGetMap(const WMSConfig& theConfig) : itsConfig(theConfig)
{
}
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
        exception.addParameter("Layere", layers[i]);
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
      Spine::Exception exception(BCP, "Invalid width / height value!", NULL);
      exception.addDetail("The WIDTH and HEIGHT options must be valid integer numbers!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      throw exception;
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

    std::string crs = *(theRequest.getParameter("CRS"));
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
    theRequest.addParameter("type", demimetype(itsParameters.format));

    // Convert WMS width & height to projection xsize & ysize
    theRequest.removeParameter("width");
    theRequest.removeParameter("height");
    theRequest.addParameter("projection.bbox", bbox);
    theRequest.addParameter("projection.xsize", Fmi::to_string(itsParameters.width));
    theRequest.addParameter("projection.ysize", Fmi::to_string(itsParameters.height));
    theRequest.addParameter("projection.crs", crs);

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
      theRequest.addParameter("time", Fmi::to_iso_string(itsParameters.timesteps[0]));
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSGetMap::jsonText() const
{
  try
  {
    const tag_map_info map_info = itsParameters.map_info_vector.back();

    return itsConfig.jsonText(map_info.name);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
