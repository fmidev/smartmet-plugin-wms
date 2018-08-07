#include "WMS.h"
#include "WMSException.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/tokenizer.hpp>
#include <spine/Convenience.h>
#include <spine/Exception.h>

#define WMS_SERVICE "WMS"
#define WMS_GET_CAPABILITIES "GetCapabilities"
#define WMS_GET_FEATURE_INFO "GetFeatureInfo"
#define WMS_GET_MAP "GetMap"
#define WMS_GET_LEGEND_GRAPHIC "GetLegendGraphic"

using boost::property_tree::ptree;

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
std::string enclose_with_quotes(const std::string& param)
{
  try
  {
    std::string retval(param);

    if (retval != "null")
    {
      retval.insert(retval.begin(), '"');
      retval.append("\"");
    }
    return retval;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Enclosing string with quotes failed!");
  }
}

rgb_color hex_string_to_rgb(const std::string& hex_string)
{
  try
  {
    // valid fromat is 0xFFFFFF
    if (hex_string.size() != 8)
    {
      throw Spine::Exception(BCP, "Invalid BGCOLOR parameter '" + hex_string + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    unsigned int red = 255;
    unsigned int green = 255;
    unsigned int blue = 255;

    try
    {
      std::size_t pos;
      red = stoi(hex_string.substr(2, 2), &pos, 16);
      green = stoi(hex_string.substr(4, 2), &pos, 16);
      blue = stoi(hex_string.substr(6, 2), &pos, 16);
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Invalid BGCOLOR parameter '" + hex_string + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    return {red, green, blue};
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Converting hex string to RGB failed!");
  }
}

WMSRequestType wmsRequestType(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    std::string service = Spine::optional_string(theHTTPRequest.getParameter("service"), "");
    std::string request = Spine::optional_string(theHTTPRequest.getParameter("request"), "");

    if (boost::iequals(request, WMS_GET_CAPABILITIES) && boost::iequals(service, WMS_SERVICE))
      return WMSRequestType::GET_CAPABILITIES;
    if (boost::iequals(request, WMS_GET_MAP))
      return WMSRequestType::GET_MAP;
    if (boost::iequals(request, WMS_GET_LEGEND_GRAPHIC))
      return WMSRequestType::GET_LEGEND_GRAPHIC;
    if (boost::iequals(request, WMS_GET_FEATURE_INFO))
      return WMSRequestType::GET_FEATURE_INFO;

    return WMSRequestType::NOT_A_WMS_REQUEST;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Extracting MWS request type failed!");
  }
}

unsigned int parse_resolution(const std::string& periodString, size_t designatorCharPos)
{
  try
  {
    if (periodString.empty() || periodString.at(0) != 'P' || designatorCharPos == 0)
    {
      Spine::Exception exception(BCP, "Cannot parse resolution string '" + periodString + "'!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      throw exception;
    }

    if (designatorCharPos == std::string::npos)
      return 0;

    size_t pos(designatorCharPos - 1);
    // count digits before previous character
    while (isdigit(periodString.at(pos)))
      pos--;

    if ((designatorCharPos - pos) == 1)
    {
      throw Spine::Exception(BCP, "Invalid dimension value '" + periodString + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
    }

    std::string numberStr(periodString.substr(pos + 1, designatorCharPos - pos - 1));

    unsigned int retval(0);

    try
    {
      retval = boost::lexical_cast<unsigned int>(numberStr);
    }
    catch (const boost::bad_lexical_cast&)
    {
      throw Spine::Exception::Trace(BCP, "Invalid dimension value '" + periodString + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
    }

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Parsing resolution failed!");
  }
}

unsigned int resolution_in_minutes(const std::string resolution)
{
  try
  {
    bool timeResolutionExists(resolution.find('T') != std::string::npos);
    bool minutesDefined(timeResolutionExists && (resolution.rfind('M') > resolution.find('T')));

    // note! years, months are not relevant in FMI timesteps, so they are not parsed
    unsigned int days(parse_resolution(resolution, resolution.find('D')));
    unsigned int hours(parse_resolution(resolution, resolution.find('H')));
    unsigned int minutes(minutesDefined ? parse_resolution(resolution, resolution.rfind('M')) : 0);
    unsigned int seconds(parse_resolution(resolution, resolution.find('S')));

    return ((days * 1440) + (hours * 60) + (minutes + seconds / 60));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Extracting resolution in minutes failed!");
  }
}

std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    std::string wmsRequest(
        (theHTTPRequest.getParameter("request") ? *(theHTTPRequest.getParameter("request")) : ""));

    ost << "SERVICE = "
        << (theHTTPRequest.getParameter("service") ? *(theHTTPRequest.getParameter("service")) : "")
        << std::endl;
    ost << "VERSION = "
        << (theHTTPRequest.getParameter("version") ? *(theHTTPRequest.getParameter("version")) : "")
        << std::endl;
    ost << "REQUEST = " << wmsRequest << std::endl;
    if (wmsRequest == "GetCapabilities")
    {
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << std::endl;
      ost << "UPDATESEQUENCE = "
          << (theHTTPRequest.getParameter("updatesequence")
                  ? *(theHTTPRequest.getParameter("updatesequence"))
                  : "")
          << std::endl;
    }
    else if (wmsRequest == "GetMap")
    {
      ost << "LAYERS = "
          << (theHTTPRequest.getParameter("layers") ? *(theHTTPRequest.getParameter("layers")) : "")
          << std::endl;
      ost << "STYLES = "
          << (theHTTPRequest.getParameter("styles") ? *(theHTTPRequest.getParameter("styles")) : "")
          << std::endl;
      ost << "CRS = "
          << (theHTTPRequest.getParameter("crs") ? *(theHTTPRequest.getParameter("crs")) : "")
          << std::endl;
      ost << "BBOX = "
          << (theHTTPRequest.getParameter("bbox") ? *(theHTTPRequest.getParameter("bbox")) : "")
          << std::endl;
      ost << "WIDTH = "
          << (theHTTPRequest.getParameter("width") ? *(theHTTPRequest.getParameter("width")) : "")
          << std::endl;
      ost << "HEIGHT = "
          << (theHTTPRequest.getParameter("height") ? *(theHTTPRequest.getParameter("height")) : "")
          << std::endl;
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << std::endl;
      ost << "TRANSPARENT = "
          << (theHTTPRequest.getParameter("transparent")
                  ? *(theHTTPRequest.getParameter("transparent"))
                  : "")
          << std::endl;
      ost << "BGCOLOR = "
          << (theHTTPRequest.getParameter("bgcolor") ? *(theHTTPRequest.getParameter("bgcolor"))
                                                     : "")
          << std::endl;
      ost << "EXCEPTIONS = "
          << (theHTTPRequest.getParameter("exceptions")
                  ? *(theHTTPRequest.getParameter("exceptions"))
                  : "")
          << std::endl;
      ost << "TIME = "
          << (theHTTPRequest.getParameter("time") ? *(theHTTPRequest.getParameter("time")) : "")
          << std::endl;
      ost << "ELEVATION = "
          << (theHTTPRequest.getParameter("elevation") ? *(theHTTPRequest.getParameter("elevation"))
                                                       : "")
          << std::endl;
    }
    else if (wmsRequest == "GetLegendGraphic")
    {
      ost << "LAYER = "
          << (theHTTPRequest.getParameter("layer") ? *(theHTTPRequest.getParameter("layer")) : "")
          << std::endl;
      ost << "STYLE = "
          << (theHTTPRequest.getParameter("style") ? *(theHTTPRequest.getParameter("style")) : "")
          << std::endl;
      ost << "REMOTE_OWS_TYPE = "
          << (theHTTPRequest.getParameter("remote_ows_type")
                  ? *(theHTTPRequest.getParameter("remote_ows_type"))
                  : "")
          << std::endl;
      ost << "REMOTE_OWS_URL = "
          << (theHTTPRequest.getParameter("remote_ows_url")
                  ? *(theHTTPRequest.getParameter("remote_ows_url"))
                  : "")
          << std::endl;
      ost << "FEATURETYPE = "
          << (theHTTPRequest.getParameter("featuretype")
                  ? *(theHTTPRequest.getParameter("featuretype"))
                  : "")
          << std::endl;
      ost << "COVERAGE = "
          << (theHTTPRequest.getParameter("coverage") ? *(theHTTPRequest.getParameter("coverage"))
                                                      : "")
          << std::endl;
      ost << "RULE = "
          << (theHTTPRequest.getParameter("rule") ? *(theHTTPRequest.getParameter("rule")) : "")
          << std::endl;
      ost << "SCALE = "
          << (theHTTPRequest.getParameter("scale") ? *(theHTTPRequest.getParameter("scale")) : "")
          << std::endl;
      ost << "SLD = "
          << (theHTTPRequest.getParameter("sld") ? *(theHTTPRequest.getParameter("sld")) : "")
          << std::endl;
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << std::endl;
      ost << "WIDTH = "
          << (theHTTPRequest.getParameter("width") ? *(theHTTPRequest.getParameter("width")) : "")
          << std::endl;
      ost << "HEIGHT = "
          << (theHTTPRequest.getParameter("height") ? *(theHTTPRequest.getParameter("height")) : "")
          << std::endl;
      ost << "EXCEPIONS = "
          << (theHTTPRequest.getParameter("exceptions")
                  ? *(theHTTPRequest.getParameter("exceptions"))
                  : "")
          << std::endl;
      ost << "SLD_VERSION = "
          << (theHTTPRequest.getParameter("sld_version")
                  ? *(theHTTPRequest.getParameter("sld_version"))
                  : "")
          << std::endl;
    }

    return ost;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Printing WMS request failed!");
  }
}

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

void handleLayerStyle(Json::Value& layerJson,
                      const Json::Value& qid,
                      const Json::Value& layer_type,
                      const Json::Value& isoLB,
                      const Json::Value& css)
{
  Json::Value nulljson;
  auto layerQid = layerJson.get("qid", nulljson);
  if (!layerQid.isNull() && qid == layerQid)
  {
    auto layerType = layerJson.get("layer_type", nulljson);
    if (layerType == layer_type && !layerType.isNull())
    {
      if (layerType.asString() == "isoline" && !isoLB.isNull())
      {
        Json::Value& isolines = layerJson["isolines"];
        isolines = isoLB;
      }
      else if (layerType.asString() == "isoband" && !isoLB.isNull())
      {
        Json::Value& isobands = layerJson["isobands"];
        isobands = isoLB;
      }
    }
    Json::Value& cssJson = layerJson["css"];
    if (!cssJson.isNull() && !css.isNull())
      cssJson = css;
  }

  auto subLayers = layerJson.get("layers", nulljson);
  if (!subLayers.isNull() && subLayers.isArray())
    for (auto& subLayer : layerJson["layers"])
      handleLayerStyle(subLayer, qid, layer_type, isoLB, css);
}

void useStyle(Json::Value& root, const Json::Value& styles)
{
  Json::Value nulljson;

  auto nameJson = styles.get("name", nulljson);
  auto titleJson = styles.get("name", nulljson);

  auto styleLayers = styles.get("layers", nulljson);
  if (!styleLayers.isNull() && styleLayers.isArray())
  {
    for (const auto& styleLayer : styleLayers)
    {
      auto qid = styleLayer.get("qid", nulljson);
      if (qid.isNull())
        continue;

      auto layer_type = styleLayer.get("layer_type", nulljson);
      std::string layerTypeString = (!layer_type.isNull() ? layer_type.asString() : "");
      auto isolines = styleLayer.get("isolines", nulljson);
      auto isobands = styleLayer.get("isobands", nulljson);
      auto css = styleLayer.get("css", nulljson);
      auto viewsJson = root.get("views", nulljson);

      if (!viewsJson.isNull() && viewsJson.isArray())
      {
        for (auto& viewJson : root["views"])
        {
          auto viewLayers = viewJson.get("layers", nulljson);
          if (!viewLayers.isNull() && viewLayers.isArray())
          {
            for (auto& layerJson : viewJson["layers"])
              handleLayerStyle(layerJson,
                               qid,
                               layer_type,
                               (layerTypeString == "isoline" ? isolines : isobands),
                               css);
          }
        }
      }
    }
  }
}

void useStyle(Json::Value& root, const std::string& styleName)
{
  if (styleName.empty())
    return;

  Json::Value nulljson;

  auto json = root.get("styles", nulljson);
  if (!json.isNull())
  {
    if (!json.isArray())
      throw Spine::Exception(BCP, "WMSLayer styles settings must be an array");

    for (const auto& styleJson : json)
    {
      const auto& nameJson = styleJson.get("name", nulljson);

      if (!nameJson.isNull() && nameJson.asString() == styleName)
      {
        useStyle(root, styleJson);
        break;
      }
    }
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
