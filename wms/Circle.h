// ======================================================================
/*!
 * \brief Circle at some coordinate
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <json/json.h>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class Circle
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  double radius = -1;
  Attributes attributes;

};  // class Circle

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
