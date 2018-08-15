#pragma once

#include <spine/HTTP.h>

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
