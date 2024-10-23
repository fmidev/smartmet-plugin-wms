// ======================================================================
/*!
 * \brief Location
 */
// ======================================================================

#pragma once

#include <json/json.h>
#include <optional>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Location
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  // See also Station.h to be consistent if you're planning to expand this class
  std::optional<double> longitude;
  std::optional<double> latitude;

  // Optional position shifts
  std::optional<int> dx;
  std::optional<int> dy;

};  // class Location

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
