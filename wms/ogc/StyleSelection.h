#pragma once

#include <json/value.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
void useStyle(Json::Value& root, const Json::Value& styles);
void useStyle(Json::Value& root, const std::string& styleName);
}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
