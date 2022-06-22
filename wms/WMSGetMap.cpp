#include "WMSGetMap.h"
#include "CaseInsensitiveComparator.h"
#include "Colour.h"
#include "Mime.h"
#include "StyleSelection.h"
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

  if (ret.find(":origintime_") != std::string::npos)
    ret = ret.substr(0, ret.find(":origintime_"));

  return ret;
}

std::string get_json_element_value(const Json::Value& json, const std::string& keyStr)
{
  std::string ret = "";

  std::vector<std::string> keys;
  boost::algorithm::split(keys, keyStr, boost::is_any_of("."), boost::token_compress_on);

  const Json::Value* jsonPtr = &json;
  for (const auto& key : keys)
  {
    if (jsonPtr->isMember(key))
    {
      jsonPtr = &((*jsonPtr)[key]);
      if (key == keys.back())
        ret = jsonPtr->asString();
    }
  }

  return ret;
}

void rename_json_element(const Json::Value& json,
                         const std::string& keyStr,
                         const std::string& postfix)
{
  std::vector<std::string> keys;

  boost::algorithm::split(keys, keyStr, boost::is_any_of("."), boost::token_compress_on);

  Json::Value nulljson;
  const Json::Value* to = &json;
  bool found = false;
  for (const auto& key : keys)
  {
    found = false;
    if (to->isMember(key))
    {
      Json::Value::Members members = to->getMemberNames();
      to = &((*to)[key]);
      found = true;
    }
  }

  if (found)
  {
    Json::Value* tototo = const_cast<Json::Value*>(to);
    *tototo = Json::Value(to->asString() + postfix);
  }
}

Json::Value merge_layers(const std::vector<Json::Value>& layers)
{
  Json::Value nulljson;

  unsigned int suffix = 1;

  for (const auto& layer : layers)
  {
    const auto& views = layer["views"];
    if (!views.isNull() && views.isArray())
    {
      for (const auto& view : views)
      {
        // Rename qid and attributes.id of the view
        std::string suffixString = Fmi::to_string(suffix);
        rename_json_element(view, "qid", suffixString);
        rename_json_element(view, "attributes.id", suffixString);

        // Rename qid and attributes.id of view's layers
        const auto& viewlayers = view["layers"];
        for (const auto& viewlayer : viewlayers)
        {
          rename_json_element(viewlayer, "qid", suffixString);
          rename_json_element(viewlayer, "attributes.id", suffixString);

          // Rename qid and attributes.id of view's sublayers
          const auto& viewsublayers = viewlayer["layers"];
          for (const auto& viewsublayer : viewsublayers)
          {
            rename_json_element(viewsublayer, "qid", suffixString);
            rename_json_element(viewsublayer, "attributes.id", suffixString);
          }
        }
        suffix++;
      }
    }
  }

  // Merge layers. Set layer #0 to the bottom and merge rest of layers on top of that.
  Json::Value ret = layers[0];
  Json::Value& retViews = ret["views"];

  // Set retDefsStyles to point to merged layer's styles attribute
  // Set retDefsLayers to point to merged layer's layers vector
  Json::Value* retDefs = nullptr;
  if (ret.isMember("defs"))
  {
    retDefs = &(ret["defs"]);
    if (!retDefs->isMember("styles"))
      (*retDefs)["styles"] = Json::Value(Json::objectValue);
    if (!retDefs->isMember("layers"))
      (*retDefs)["layers"] = Json::Value(Json::arrayValue);
  }
  else
  {
    ret["defs"] = Json::Value(Json::objectValue);
    retDefs = &(ret["defs"]);
    (*retDefs)["styles"] = Json::Value(Json::objectValue);
    (*retDefs)["layers"] = Json::Value(Json::arrayValue);
  }
  Json::Value& retDefsStyles = (*retDefs)["styles"];
  Json::Value& retDefsLayers = (*retDefs)["layers"];

  // Styles on WMS layer (can be changed in URL)
  if (!ret.isMember("styles"))
    ret["styles"] = Json::Value(Json::arrayValue);
  Json::Value& retStyles = ret["styles"];

  if (!ret.isMember("refs"))
    ret["refs"] = Json::Value(Json::objectValue);
  Json::Value& retRefs = ret["refs"];

  // Store ids of layers in defs-section of first layer
  std::set<std::string> defsLayerIdSet;
  for (const auto& layer : retDefsLayers)
  {
    std::string layerId = get_json_element_value(layer, "attributes.id");
    if (!layerId.empty())
      defsLayerIdSet.insert(layerId);
  }

  // Iterate rest of the layers in defs-section
  for (unsigned int i = 1; i < layers.size(); i++)
  {
    const Json::Value& fromLayer = layers[i];
    // Merge defs
    if (fromLayer.isMember("defs"))
    {
      Json::Value fromDefs = fromLayer.get("defs", nulljson);
      // Merge layer styles
      if (fromDefs.isMember("styles"))
      {
        Json::Value::Members currentStyleNames = retDefsStyles.getMemberNames();
        std::set<std::string> currentStyleNameSet(currentStyleNames.begin(),
                                                  currentStyleNames.end());
        const Json::Value& fromDefsStyles = fromDefs["styles"];
        Json::Value::Members fromStyleMemberNames = fromDefsStyles.getMemberNames();
        for (const auto& stylename : fromStyleMemberNames)
        {
          // If style with same name does not exist add it
          if (currentStyleNameSet.find(stylename) == currentStyleNameSet.end())
          {
            (retDefsStyles)[stylename] = fromDefsStyles[stylename];
          }
        }
      }
      // Merge layers inside defs, if layer with the same id exist dont merge
      if (fromDefs.isMember("layers"))
      {
        const auto& defsLayers = fromDefs["layers"];
        for (const auto& layer : defsLayers)
        {
          std::string layerId = get_json_element_value(layer, "attributes.id");
          if (layerId.empty() || defsLayerIdSet.find(layerId) == defsLayerIdSet.end())
          {
            retDefsLayers.append(layer);
            defsLayerIdSet.insert(layerId);
          }
        }
      }
    }

    // Merge refs
    if (fromLayer.isMember("refs"))
    {
      Json::Value fromRefs = fromLayer.get("refs", nulljson);
      Json::Value::Members fromRefNames = fromRefs.getMemberNames();
      Json::Value::Members currentRefNames = retRefs.getMemberNames();
      std::set<std::string> currentRefNameSet(currentRefNames.begin(), currentRefNames.end());
      for (const auto& refname : fromRefNames)
      {
        // If ref with same name does not exist add it
        if (currentRefNameSet.find(refname) == currentRefNameSet.end())
        {
          (retRefs)[refname] = fromRefs[refname];
        }
      }
    }

    // Merge styles
    if (fromLayer.isMember("styles"))
    {
      Json::Value fromStyles = fromLayer.get("styles", nulljson);
      if (!fromStyles.isNull() && fromStyles.isArray())
      {
        for (const auto& fromStyle : fromStyles)
        {
          retStyles.append(fromStyle);
        }
      }
    }

    // Merge views, just append one after another
    auto fromViews = fromLayer.get("views", nulljson);
    if (!fromViews.isNull() && fromViews.isArray())
    {
      for (const auto& fromView : fromViews)
      {
        retViews.append(fromView);
      }
    }
  }

  return ret;
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
          .disableStackTrace();
    }

    if (!theHTTPRequest.getParameter("LAYERS"))
    {
      throw Fmi::Exception(BCP, "At least one layer must be defined in GetMap request")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
          .disableStackTrace();
    }

    if (!theHTTPRequest.getParameter("STYLES"))
    {
      throw Fmi::Exception(BCP, "STYLES-option must be defined, even if it is empty")
          .addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED)
          .disableStackTrace();
    }

    if (!theHTTPRequest.getParameter("CRS"))
    {
      throw Fmi::Exception(BCP, "CRS-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE)
          .disableStackTrace();
    }

    if (!theHTTPRequest.getParameter("BBOX"))
    {
      throw Fmi::Exception(BCP, "BBOX-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE)
          .disableStackTrace();
    }

    if (!theHTTPRequest.getParameter("WIDTH"))
    {
      throw Fmi::Exception(BCP, "WIDTH-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE)
          .disableStackTrace();
    }

    if (!theHTTPRequest.getParameter("HEIGHT"))
    {
      throw Fmi::Exception(BCP, "HEIGHT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_MISSING_DIMENSION_VALUE)
          .disableStackTrace();
    }

    if (!theHTTPRequest.getParameter("FORMAT"))
    {
      throw Fmi::Exception(BCP, "FORMAT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE)
          .disableStackTrace();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetMap request options did not validate!");
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
      throw Fmi::Exception(BCP, "The requested version is not supported!")
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
              .disableStackTrace();
        }
      }

      // check that given timesteps are valid
      for (const boost::posix_time::ptime& timestamp : options.timesteps)
      {
        if (!itsConfig.isValidTime(layer, timestamp, options.reference_time))
        {
          // TODO: enable when FMI app is working properly
          throw Fmi::Exception(BCP, "Invalid time requested!")
              .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
              .addParameter("Requested time", Fmi::to_iso_string(timestamp))
              .addParameter("Requested layer", layer)
              .disableStackTrace();
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
              .disableStackTrace();
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
          .addParameter("xMin", std::to_string(options.bbox.xMin))
          .addParameter("yMin", std::to_string(options.bbox.yMin))
          .addParameter("xMax", std::to_string(options.bbox.xMax))
          .addParameter("yMax", std::to_string(options.bbox.yMax));
    }

    // check interval dimesion
    if (options.interval_start || options.interval_end)
    {
      int interval_start = abs(options.interval_start ? *options.interval_start : 0);
      int interval_end = abs(options.interval_end ? *options.interval_end : 0);

      for (unsigned int i = 0; i < options.map_info_vector.size(); i++)
      {
        std::string layer(options.map_info_vector[i].name);
        if (!itsConfig.isValidInterval(layer, interval_start, interval_end))
        {
          std::string interval =
              (Fmi::to_string(interval_start) + ", " + Fmi::to_string(interval_end));
          throw Fmi::Exception(BCP,
                               "Invalid interval requested for layer " + layer +
                                   "! interval_start, interval_end: " + interval)
              .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE)
              .disableStackTrace();
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
    throw Fmi::Exception::Trace(BCP, "Parsing GetMap time failed!");
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
      Fmi::Exception exception(BCP, "Invalid time interval!");
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
        startTime += boost::posix_time::minutes(total_minutes);
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
    for (unsigned int i = 0; i < layers.size(); i++)
    {
      std::string layername = layers[i];
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
    if (styles_opt.size() == 0)
      styles.insert(styles.begin(), layers.size(), "");

    // there must be one-to-one relationship between layers and styles
    // so if styles has been defined there must be equal amount of layers
    if (styles.size() != layers.size())
    {
      Fmi::Exception exception(BCP, "LAYERS and STYLES amount mismatch");
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
      throw Fmi::Exception::Trace(BCP, "Invalid width / height value!")
          .addDetail("The WIDTH and HEIGHT options must be valid integer numbers!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    for (unsigned int i = 0; i < layers.size(); i++)
    {
      std::string layerName(layer_name(layers[i]));

      if (!itsConfig.isValidLayer(layerName))
      {
        Fmi::Exception exception(BCP, "The requested layer is not supported!");
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

    std::string layerName(layers.size() > 0 ? layer_name(layers.back()) : "");

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
          Fmi::Exception exception(BCP, "Invalid TIME option value for the current layer!");
          exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
          exception.addParameter("Time value", time_str);
          exception.addParameter("Layer", layerName);
          throw exception;
        }

        boost::posix_time::ptime mostCurrentTime(
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
    throw Fmi::Exception::Trace(BCP, "Failed to complete GetMap request!");
  }
}

Json::Value WMSGetMap::json() const
{
  try
  {
    std::vector<Json::Value> jsonlayers;
    for (const auto& map_info : itsParameters.map_info_vector)
    {
      Json::Value json = itsConfig.json(map_info.name);
      if (!map_info.style.empty())
      {
        SmartMet::Plugin::WMS::useStyle(json, map_info.style);
      }
      jsonlayers.push_back(json);
    }

    if (jsonlayers.size() == 1)
      return jsonlayers.back();

    return merge_layers(jsonlayers);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Extracting GetMap JSON failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
