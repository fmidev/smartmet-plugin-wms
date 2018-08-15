#pragma once

#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
enum class WMSRequestType
{
  GET_CAPABILITIES,
  GET_MAP,
  GET_LEGEND_GRAPHIC,
  GET_FEATURE_INFO,
  NOT_A_WMS_REQUEST
};

WMSRequestType wmsRequestType(const Spine::HTTP::Request& theHTTPRequest);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
