// ======================================================================
/*!
 * \brief A Meb Maps Service (WMS) Request
 */
// ======================================================================

#pragma once

#include <boost/shared_ptr.hpp>
#include <spine/HTTP.h>
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest);

void useStyle(Json::Value& root, const Json::Value& styles);
void useStyle(Json::Value& root, const std::string& styleName);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
