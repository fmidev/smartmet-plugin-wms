// ======================================================================
/*!
 * \brief Locations
 */
// ======================================================================

#pragma once

#include "Location.h"
#include <json/forwards.h>
#include <cstddef>
#include <vector>

namespace SmartMet::Plugin::Dali
{
class State;
}  // namespace SmartMet::Plugin::Dali

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class Locations
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::vector<Location> locations;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
