#include "WMS.h"
#include "WMSException.h"

#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/tokenizer.hpp>
#include <spine/Convenience.h>
#include <spine/Exception.h>

#define WMS_SERVICE "WMS"
#define WMS_GET_CAPABILITIES "GetCapabilities"
#define WMS_GET_FEATURE_INFO "GetFeatureInfo"
#define WMS_GET_MAP "GetMap"

using namespace std;
using namespace boost;
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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

    int red = 255;
    int green = 255;
    int blue = 255;

    try
    {
      std::size_t pos;
      red = stoi(hex_string.substr(2, 2), &pos, 16);
      green = stoi(hex_string.substr(4, 2), &pos, 16);
      blue = stoi(hex_string.substr(6, 2), &pos, 16);
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "Invalid BGCOLOR parameter '" + hex_string + "'!", NULL)
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    return rgb_color(red, green, blue);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

WMSRequestType wmsRequestType(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    std::string service = Spine::optional_string(theHTTPRequest.getParameter("service"), "");
    std::string request = Spine::optional_string(theHTTPRequest.getParameter("request"), "");

    if (request == WMS_GET_CAPABILITIES && boost::iequals(service, WMS_SERVICE))
      return WMSRequestType::GET_CAPABILITIES;
    else if (request == WMS_GET_MAP)
      return WMSRequestType::GET_MAP;
    else if (request == WMS_GET_FEATURE_INFO)
      return WMSRequestType::GET_FEATURE_INFO;

    return WMSRequestType::NOT_A_WMS_REQUEST;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    catch (boost::bad_lexical_cast const&)
    {
      throw Spine::Exception(BCP, "Invalid dimension value '" + periodString + "'!", NULL)
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
    }

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

unsigned int resolution_in_minutes(const std::string resolution)
{
  try
  {
    bool timeResolutionExists(resolution.find("T") != std::string::npos);
    bool minutesDefined(timeResolutionExists && (resolution.rfind("M") > resolution.find("T")));

    // note! years, months are not relevant in FMI timesteps, so they are not parsed
    unsigned int days(parse_resolution(resolution, resolution.find("D")));
    unsigned int hours(parse_resolution(resolution, resolution.find("H")));
    unsigned int minutes(minutesDefined ? parse_resolution(resolution, resolution.rfind("M")) : 0);
    unsigned int seconds(parse_resolution(resolution, resolution.find("S")));

    return ((days * 1440) + (hours * 60) + (minutes + seconds / 60));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      ost << "UPDATESEQUENCE = " << (theHTTPRequest.getParameter("updatesequence")
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
      ost << "TRANSPARENT = " << (theHTTPRequest.getParameter("transparent")
                                      ? *(theHTTPRequest.getParameter("transparent"))
                                      : "")
          << std::endl;
      ost << "BGCOLOR = " << (theHTTPRequest.getParameter("bgcolor")
                                  ? *(theHTTPRequest.getParameter("bgcolor"))
                                  : "")
          << std::endl;
      ost << "EXCEPTIONS = " << (theHTTPRequest.getParameter("exceptions")
                                     ? *(theHTTPRequest.getParameter("exceptions"))
                                     : "")
          << std::endl;
      ost << "TIME = "
          << (theHTTPRequest.getParameter("time") ? *(theHTTPRequest.getParameter("time")) : "")
          << std::endl;
      ost << "ELEVATION = " << (theHTTPRequest.getParameter("elevation")
                                    ? *(theHTTPRequest.getParameter("elevation"))
                                    : "")
          << std::endl;
    }

    return ost;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
