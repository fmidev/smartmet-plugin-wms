#include "WMSGetMap.h"
#include "CaseInsensitiveComparator.h"
#include "Colour.h"
#include "Mime.h"
#include "TimeResolution.h"
#include "WMSException.h"
#include <boost/algorithm/string/erase.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
bool is_number(const std::string& str)
{
  auto it = str.begin();
  if (*it == '-' || *it == '+')
    it++;
  while (it != str.end() && std::isdigit(*it))
  {
    it++;
  }
  return !str.empty() && it == str.end();
}

std::string layer_name(const std::string& name)
{
  std::string ret = name;

  auto pos = ret.find(":origintime_");
  if (pos != std::string::npos)
    ret.resize(pos);

  return ret;
}

std::string get_crs(const Spine::HTTP::Request& request)
{
  auto tmp = request.getParameter("CRS");  // WMS 1.3
  if (tmp)
    return *tmp;
  tmp = request.getParameter("SRS");  // WMS 1.1
  if (tmp)
    return *tmp;

  throw Fmi::Exception(BCP, "CRS-option has not been defined")
      .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE)
      .disableLogging();
}

void check_getmap_request_options(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    // check that all mandatory options has been defined
    std::string missing_options;

    if (!theHTTPRequest.getParameter("VERSION"))
    {
      throw Fmi::Exception(BCP, "Version not defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE)
          .disableLogging();
    }

    if (!theHTTPRequest.getParameter("LAYERS"))
    {
      throw Fmi::Exception(BCP, "At least one layer must be defined in GetMap request")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
          .disableLogging();
    }

    if (!theHTTPRequest.getParameter("STYLES"))
    {
      throw Fmi::Exception(BCP, "STYLES-option must be defined, even if it is empty")
          .addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED)
          .disableLogging();
    }

    if (!theHTTPRequest.getParameter("CRS") && !theHTTPRequest.getParameter("SRS"))
    {
      throw Fmi::Exception(BCP, "CRS-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE)
          .disableLogging();
    }

    if (!theHTTPRequest.getParameter("BBOX"))
    {
      throw Fmi::Exception(BCP, "BBOX-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE)
          .disableLogging();
    }

    if (!theHTTPRequest.getParameter("WIDTH"))
    {
      throw Fmi::Exception(BCP, "WIDTH-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE)
          .disableLogging();
    }

    if (!theHTTPRequest.getParameter("HEIGHT"))
    {
      throw Fmi::Exception(BCP, "HEIGHT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE)
          .disableLogging();
    }

    if (!theHTTPRequest.getParameter("FORMAT"))
    {
      throw Fmi::Exception(BCP, "FORMAT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE)
          .disableLogging();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetMap request options did not validate!");
  }
}

void validate_options(const tag_get_map_request_options& options,
                      const WMSConfig& itsConfig,
                      const Engine::Querydata::Engine& /* querydata */)
{
  try
  {
    // check the requested version number
    if (!itsConfig.isValidVersion(options.version))
    {
      throw Fmi::Exception(BCP, "The requested version is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED)
          .addParameter("Requested version", options.version)
          .addParameter("Supported versions",
                        boost::algorithm::join(itsConfig.supportedWMSVersions(), "','"))
          .disableLogging();
    }

    // check whether the requested layers, styles, CRS are valid
    for (unsigned int i = 0; i < options.map_info_vector.size(); i++)
    {
      std::string layer = options.map_info_vector[i].name;
      std::string style = options.map_info_vector[i].style;

      // check that layer is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidLayer(layer))
      {
        throw Fmi::Exception(BCP, "The requested layer is not supported!")
            .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
            .addParameter("Requested layer", layer);
      }

      // check that style is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidStyle(layer, style))
      {
        throw Fmi::Exception(BCP, "The style is not supported by the requested layer!")
            .addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED)
            .addParameter("Requested style", style)
            .addParameter("Requested layer", layer);
      }

      // check that CRS is valid (as defined in GetCapabilities response)
      if (!itsConfig.isValidCRS(layer, options.bbox.crs))
      {
        throw Fmi::Exception(BCP, "The CRS is not supported by the requested layer!")
            .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_CRS)
            .addParameter("Requested CRS", options.bbox.crs)
            .addParameter("Requested layer", layer);
      }

      // check reference time
      if (options.reference_time)
      {
        if (!itsConfig.isValidReferenceTime(layer, *options.reference_time))
        {
          throw Fmi::Exception(BCP, "Invalid reference time requested!")
              .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
              .addParameter("Requested reference time", Fmi::to_iso_string(*options.reference_time))
              .addParameter("Requested layer", layer)
              .disableLogging();
        }
      }

      // check that given timesteps are valid
      for (const Fmi::DateTime& timestamp : options.timesteps)
      {
        if (!itsConfig.isValidTime(layer, timestamp, options.reference_time))
        {
          // TODO: enable when FMI app is working properly
          throw Fmi::Exception(BCP, "Invalid time requested!")
              .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
              .addParameter("Requested time", Fmi::to_iso_string(timestamp))
              .addParameter("Requested layer", layer)
              .disableLogging();
        }
      }

      // check that given elevation is valid
      if (options.elevation)
      {
        if (!itsConfig.isValidElevation(layer, *options.elevation))
        {
          throw Fmi::Exception(BCP, "Invalid elevation requested!")
              .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
              .addParameter("Requested elevation", Fmi::to_string(*options.elevation))
              .addParameter("Requested layer", layer)
              .disableLogging();
        }
      }
    }

    // check format
    if (!itsConfig.isValidMapFormat(options.format))
    {
      throw Fmi::Exception(BCP, "The requested map format is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_FORMAT)
          .addParameter("Requested map format", options.format)
          .addParameter("Supported map formats",
                        boost::algorithm::join(itsConfig.supportedMapFormats(), "','"));
    }

    // check bbox
    if (options.bbox.xMin > options.bbox.xMax || options.bbox.yMin > options.bbox.yMax)
    {
      throw Fmi::Exception(BCP, "Invalid BBOX definition!")
          .addDetail("'xMin' must be smaller than 'xMax' and 'yMin' must be smaller than 'yMax'.")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
          .addParameter("xMin", Fmi::to_string(options.bbox.xMin))
          .addParameter("yMin", Fmi::to_string(options.bbox.yMin))
          .addParameter("xMax", Fmi::to_string(options.bbox.xMax))
          .addParameter("yMax", Fmi::to_string(options.bbox.yMax));
    }

    // check interval dimesion
    if (options.interval_start || options.interval_end)
    {
      int interval_start = abs(options.interval_start ? *options.interval_start : 0);
      int interval_end = abs(options.interval_end ? *options.interval_end : 0);

      for (const auto& map_info : options.map_info_vector)
      {
        const auto& layer = map_info.name;
        if (!itsConfig.isValidInterval(layer, interval_start, interval_end))
        {
          std::string interval =
              (Fmi::to_string(interval_start) + ", " + Fmi::to_string(interval_end));
          throw Fmi::Exception(BCP,
                               "Invalid interval requested for layer " + layer +
                                   "! interval_start, interval_end: " + interval)
              .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
              .disableLogging();
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetMap options did not validate!");
  }
}

// if iso-string contains fractions of seconds remove them
// so that Fmi::TimeParser knows how to parse
Fmi::DateTime parse_time(const std::string& time)
{
  try
  {
    std::string t(time);
    // remove second fractions
    if (t.find('Z') != std::string::npos && t.find('.') != std::string::npos &&
        t.find('Z') > t.find('.'))  // iso string with fractions of second
    {
      size_t len(t.find('Z') - t.find('.'));
      t.erase(t.find('.'), len);
    }

    return Fmi::TimeParser::parse(t);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Parsing GetMap time failed!");
  }
}

void parse_interval_with_resolution(const std::string& time_str,
                                    std::vector<Fmi::DateTime>& timesteps)
{
  try
  {
    // interval defined by its lower bound, upper bound and resolution: min/max/res
    std::vector<std::string> parts;
    boost::algorithm::split(parts, time_str, boost::algorithm::is_any_of("/"));

    if (parts.size() != 3)
    {
      Fmi::Exception exception(BCP, "Invalid time interval!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      exception.addParameter("Time interval", time_str);
      throw exception;
    }

    CaseInsensitiveComparator cicomp;
    Fmi::DateTime startTime =
        (cicomp(parts[0], "current") ? Fmi::SecondClock::universal_time() : parse_time(parts[0]));
    Fmi::DateTime endTime =
        (cicomp(parts[1], "current") ? Fmi::SecondClock::universal_time() : parse_time(parts[1]));

    if (endTime < startTime)
    {
      Fmi::Exception exception(BCP, "Invalid time interval!");
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
        startTime += Fmi::Minutes(total_minutes);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Parsing GetMap time interval failed!");
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

    // Must define at least one layer
    if (layers.empty())
      throw Fmi::Exception(BCP, "LAYERS option not set")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);

    std::string layer_origintime;
    for (const auto& layername : layers)
    {
      if (!itsConfig.isValidLayer(layer_name(layername)))
      {
        Fmi::Exception exception(BCP, "The requested layer is not supported!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
        exception.addParameter("Layer", layername);
        throw exception;
      }

      if (layername.find(":origintime_") != std::string::npos)
        layer_origintime = layername.substr(layername.find(":origintime_") + 12);
    }

    if (!styles_opt.empty())
      boost::algorithm::split(styles, styles_opt, boost::algorithm::is_any_of(","));

    // if empty STYLES definition, fill the vector with default style names
    if (styles_opt.empty())
      styles.insert(styles.begin(), layers.size(), "");

    // there must be one-to-one relationship between layers and styles
    // so if styles has been defined there must be equal amount of layers
    if (styles.size() != layers.size())
    {
      Fmi::Exception exception(BCP, "LAYERS and STYLES amount mismatch");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      exception.addParameter("Layers", Fmi::to_string(layers.size()));
      exception.addParameter("Styles", Fmi::to_string(styles.size()));
      throw exception;
    }

    // width and height
    try
    {
      itsParameters.width = Fmi::stoul(*theRequest.getParameter("width"));
      itsParameters.height = Fmi::stoul(*theRequest.getParameter("height"));
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Invalid width / height value!")
          .addDetail("The WIDTH and HEIGHT options must be valid integer numbers!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    if (itsParameters.width < 2 || itsParameters.height < 2)
      throw Fmi::Exception(BCP, "Image size must be at least 2x2");

    if (static_cast<double>(itsParameters.width) * static_cast<double>(itsParameters.height) >
        itsConfig.getDaliConfig().maxImageSize())
    {
      Fmi::Exception exception(BCP, "Too large width*height value");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      throw exception;
    }

    for (unsigned int i = 0; i < layers.size(); i++)
    {
      std::string layerName = layer_name(layers[i]);

      std::string layerCustomer = itsConfig.layerCustomer(layerName);
      std::string layerStyle = styles[i];

      itsParameters.map_info_vector.emplace_back(tag_map_info(layerName, layerStyle));
    }

    std::string crs = get_crs(theRequest);

    std::string bbox = *(theRequest.getParameter("BBOX"));

    itsParameters.bbox = Spine::BoundingBox(bbox + "," + crs);

    itsParameters.format = *(theRequest.getParameter("FORMAT"));

    CaseInsensitiveComparator cicomp;

    std::string transparent_str(
        Spine::optional_string(theRequest.getParameter("TRANSPARENT"), "FALSE"));
    if (!cicomp(transparent_str, "true") && !cicomp(transparent_str, "false"))
    {
      Fmi::Exception exception(BCP, "Invalid value for the TRANSPARENT option!");
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

    if (theRequest.getParameter("ELEVATION"))
    {
      itsParameters.elevation = Spine::optional_int(theRequest.getParameter("ELEVATION"), 0);
    }

    if (theRequest.getParameter("DIM_REFERENCE_TIME") || theRequest.getParameter("ORIGINTIME") ||
        !layer_origintime.empty())
    {
      std::string reference_time =
          Spine::optional_string(theRequest.getParameter("DIM_REFERENCE_TIME"), "");
      std::string origintime = Spine::optional_string(theRequest.getParameter("ORIGINTIME"), "");
      if (reference_time.empty() && !origintime.empty())
        reference_time = origintime;

      if (reference_time.empty())
      {
        // Use origintime from layer name
        reference_time = layer_origintime;
      }

      if (!reference_time.empty())
        itsParameters.reference_time = parse_time(reference_time);

      // reference_time and origintime are the same thing, dali understands origintime-parameter
      if (origintime.empty() && !reference_time.empty())
        theRequest.addParameter("origintime", reference_time);
    }

    if (theRequest.getParameter("TIME"))
    {
      std::string time_str = Spine::optional_string(theRequest.getParameter("TIME"), "current");

      if (cicomp(time_str, "current"))
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

        for (const auto& timestep : timesteps)
          itsParameters.timesteps.push_back(cicomp(timestep, "current")
                                                ? Fmi::SecondClock::universal_time()
                                                : parse_time(timestep));
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

        for (const auto& part : parts)
          parse_interval_with_resolution(part, itsParameters.timesteps);
      }
      else
      {
        Fmi::Exception exception(BCP, "Invalid TIME option value!");
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

    // Check DIM_INTERVAL_START and DIM_INTERVAL_END
    std::string dim_interval_start =
        Spine::optional_string(theRequest.getParameter("DIM_INTERVAL_START"), "");
    std::string dim_interval_end =
        Spine::optional_string(theRequest.getParameter("DIM_INTERVAL_END"), "");
    boost::algorithm::trim(dim_interval_start);
    boost::algorithm::trim(dim_interval_end);
    boost::replace_all(dim_interval_start, " ", "");
    boost::replace_all(dim_interval_end, " ", "");

    if (!dim_interval_start.empty())
    {
      if (!is_number(dim_interval_start))
      {
        Fmi::Exception exception(BCP, "Invalid DIM_INTERVAL_START option value, must be integer!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
        exception.addParameter("Interval value", dim_interval_start);
        throw exception;
      }
      itsParameters.interval_start = Fmi::stoi(dim_interval_start);
    }
    if (!dim_interval_end.empty())
    {
      if (!is_number(dim_interval_end))
      {
        Fmi::Exception exception(BCP, "Invalid DIM_INTERVAL_END option value, must be integer!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
        exception.addParameter("Interval value", dim_interval_end);
        throw exception;
      }
      itsParameters.interval_end = Fmi::stoi(dim_interval_end);
    }

    // validate the given options
    validate_options(itsParameters, itsConfig, theQEngine);

    std::string layerName(!layers.empty() ? layer_name(layers.back()) : "");

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

    // Add interval if exists
    if (itsParameters.interval_start || itsParameters.interval_end)
    {
      int interval_start = (itsParameters.interval_start ? *itsParameters.interval_start : 0);
      int interval_end = (itsParameters.interval_end ? *itsParameters.interval_end : 0);

      theRequest.removeParameter("DIM_INTERVAL_START");
      theRequest.removeParameter("DIM_INTERVAL_END");
      theRequest.addParameter("interval_start", Fmi::to_string(abs(interval_start)));
      theRequest.addParameter("interval_end", Fmi::to_string(abs(interval_end)));
    }
    else
    {
      // Else check if layer has default interval configured
      auto default_interval = itsConfig.getDefaultInterval(layerName);
      auto interval_start = default_interval.first;
      auto interval_end = default_interval.second;
      if (!interval_start.empty() || !interval_end.empty())
      {
        if (interval_start.empty())
          interval_start = "0";
        if (interval_end.empty())
          interval_end = "0";
        theRequest.addParameter("interval_start", interval_start);
        theRequest.addParameter("interval_end", interval_end);
      }
    }

    // This must be done after the validate_options call or we will not get the
    // correct exception as output

    std::string crs_decl = itsConfig.getCRSDefinition(crs);  // Pass GDAL string to renderer
    theRequest.addParameter("projection.crs", crs_decl);

    // Bounding box should always be defined using the main crs

    theRequest.addParameter("customer", itsConfig.layerCustomer(layerName));

    // resolve current time (most recent) for the layer
    std::string time_str = Spine::optional_string(theRequest.getParameter("TIME"), "current");

    if (cicomp(time_str, "current"))
    {
      if (!itsConfig.isTemporal(layerName))
      {
        theRequest.removeParameter("time");
      }
      else
      {
        if (!itsConfig.currentValue(layerName))
        {
          Fmi::Exception exception(BCP, "Invalid TIME option value for the current layer!");
          exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
          exception.addParameter("Time value", time_str);
          exception.addParameter("Layer", layerName);
          throw exception;
        }

        Fmi::DateTime mostCurrentTime(
            itsConfig.mostCurrentTime(layerName, itsParameters.reference_time));

        if (mostCurrentTime.is_not_a_date_time())
          theRequest.removeParameter("time");
        else
          theRequest.addParameter("time", Fmi::to_iso_string(mostCurrentTime));
      }
    }
    else
    {
      if (itsParameters.timesteps.empty())
        throw Fmi::Exception(BCP, "Intervals need to be at least one minute long");
      theRequest.removeParameter("time");
      theRequest.addParameter("time", Fmi::to_iso_string(itsParameters.timesteps[0]));
    }
  }
  catch (...)
  {
    bool quiet = itsConfig.getDaliConfig().quiet();
    Fmi::Exception ex(BCP, "Invalid GetMap input parameters", nullptr);
    if (!itsParameters.debug)
      ex.disableStackTrace();  // full stack trace not useful for user input errors unless debugging
    if (quiet)
      ex.disableLogging();  // no journaling of user errors in release servers
    throw ex;
  }
}

std::vector<Json::Value> WMSGetMap::jsons() const
{
  try
  {
    std::vector<Json::Value> jsonlayers;
    for (const auto& map_info : itsParameters.map_info_vector)
    {
      Json::Value json = itsConfig.json(map_info.name);
      jsonlayers.push_back(json);
    }
    return jsonlayers;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Extracting GetMap JSON failed!");
  }
}

std::vector<std::string> WMSGetMap::styles() const
{
  try
  {
    std::vector<std::string> ret;
    for (const auto& map_info : itsParameters.map_info_vector)
      ret.push_back(map_info.style);
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Extracting GetMap styles failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
