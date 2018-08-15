#pragma once

#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
unsigned int parse_resolution(const std::string& periodString, size_t designatorCharPos);
unsigned int resolution_in_minutes(const std::string& resolution);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
