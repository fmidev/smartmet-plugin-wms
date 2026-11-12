#pragma once

namespace SmartMet::Spine::HTTP
{
class Request;
}  // namespace SmartMet::Spine::HTTP

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
enum class RequestType
{
  GET_CAPABILITIES,
  GET_MAP,
  GET_LEGEND_GRAPHIC,
  GET_FEATURE_INFO,
  NOT_A_WMS_REQUEST
};

RequestType requestType(const Spine::HTTP::Request& theHTTPRequest);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
