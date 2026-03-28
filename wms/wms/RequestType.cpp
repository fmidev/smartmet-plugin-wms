#include "RequestType.h"
#include <macgyver/Exception.h>
#include <spine/Convenience.h>

#define WMS_SERVICE "WMS"
#define WMS_GET_CAPABILITIES "GetCapabilities"
#define WMS_GET_FEATURE_INFO "GetFeatureInfo"
#define WMS_GET_MAP "GetMap"
#define WMS_GET_LEGEND_GRAPHIC "GetLegendGraphic"

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
RequestType requestType(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    std::string service = Spine::optional_string(theHTTPRequest.getParameter("service"), "");
    std::string request = Spine::optional_string(theHTTPRequest.getParameter("request"), "");

    if (boost::iequals(request, WMS_GET_CAPABILITIES) && boost::iequals(service, WMS_SERVICE))
      return RequestType::GET_CAPABILITIES;
    if (boost::iequals(request, WMS_GET_MAP))
      return RequestType::GET_MAP;
    if (boost::iequals(request, WMS_GET_LEGEND_GRAPHIC))
      return RequestType::GET_LEGEND_GRAPHIC;
    if (boost::iequals(request, WMS_GET_FEATURE_INFO))
      return RequestType::GET_FEATURE_INFO;

    return RequestType::NOT_A_WMS_REQUEST;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Extracting MWS request type failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
