// ======================================================================
/*!
 * \brief Stations
 */
// ======================================================================

#pragma once

#include "Station.h"
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

class Stations
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;
  bool empty() const { return stations.empty(); }

  std::vector<Station> stations;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
