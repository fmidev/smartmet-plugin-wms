// ======================================================================
/*!
 * \brief Map layer styling based on features
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include <map>
#include <optional>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class MapStyles
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);

  std::size_t hash_value(const State& theState) const;

  std::string field;                    // database field identifying the area
  std::string parameter;                // weather parameter whose value to pick
  std::map<std::string, int> features;  // name to station map for fetching data value
  std::map<std::string, Attributes> feature_attributes;  // polygon specific attributes
  std::vector<AttributeSelection> data_attributes;       // map data value to SVG style settings

 private:
};  // class MapStyles

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
