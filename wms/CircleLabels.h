// ======================================================================
/*!
 * \brief Circle at some coordinate
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class CircleLabels
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;
  bool empty() const { return layout.empty(); }
  std::string format(double value) const;

  std::set<std::string> layout;  // left, right, top, bottom, east, west, north, south
  Attributes attributes;         // for all labels
  Attributes textattributes;     // for individual labels
  int dx = 0;                    // for minor coordinate adjustments
  int dy = 0;                    // ... baseline settings have issues

 private:
  std::string prefix;  // for example "R = "
  std::string suffix;  // for example " km"

};  // class Circle

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
