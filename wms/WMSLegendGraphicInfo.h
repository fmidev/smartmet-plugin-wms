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
  bool empty() const { return info.empty(); }
  void add(const std::string& name, const Json::Value& val) { info.insert(make_pair(name, val)); }
  unsigned int labelWidth(const std::string& language) const
  {
    if (text_lengths.find(language) != text_lengths.end())
      return text_lengths.at(language);
    return 0;
  }

  std::map<std::string, Json::Value> info;
  std::map<std::string, unsigned int> text_lengths;
};

using LegendGraphicInfo = std::vector<LegendGraphicInfoItem>;
using NamedLegendGraphicInfo = std::map<std::string, LegendGraphicInfo>;

struct LegendGraphicResult
{
  unsigned int width{10};
  unsigned int height{10};
  std::vector<std::string> legendLayers;
};

using LegendGraphicResultPerLanguage = std::map<std::string, LegendGraphicResult>;

std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfoItem& lgi);
std::ostream& operator<<(std::ostream& ost, const NamedLegendGraphicInfo& nlgi);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicResultPerLanguage& lgr);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
