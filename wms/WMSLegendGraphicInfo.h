// ======================================================================
/*!
 * \brief Data structures for LegendGraphic
 */
// ======================================================================

#pragma once

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct LegendGraphicInfoItem
{
  std::string asString(const std::string& name) const
  {
    return (info.find(name) != info.end() ? info.at(name).asString() : std::string());
  }
  std::string toStyledString(const std::string& name) const
  {
    return (info.find(name) != info.end() ? info.at(name).toStyledString() : std::string());
  }
  Json::Value asJsonValue(const std::string& name) const
  {
    return (info.find(name) != info.end() ? info.at(name) : Json::Value());
  }
  bool exists(const std::string& name) const { return (info.find(name) != info.end()); }
  bool empty() const { return (info.size() == 0); }
  void add(const std::string& name, const Json::Value& val) { info.insert(make_pair(name, val)); }

  std::map<std::string, Json::Value> info;
};

using LegendGraphicInfo = std::vector<LegendGraphicInfoItem>;
using NamedLegendGraphicInfo = std::map<std::string, LegendGraphicInfo>;

struct LegendGraphicResult
{
  unsigned int width;
  unsigned int height;
  std::vector<std::string> legendLayers;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
