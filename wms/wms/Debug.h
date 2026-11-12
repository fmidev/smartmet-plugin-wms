#pragma once

#include <iosfwd>

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
std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
