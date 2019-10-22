#pragma once

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
void useStyle(Json::Value& root, const Json::Value& styles);
void useStyle(Json::Value& root, const std::string& styleNames);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
