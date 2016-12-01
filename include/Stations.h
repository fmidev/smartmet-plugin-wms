// ======================================================================
/*!
 * \brief Stations
 */
// ======================================================================

#pragma once

#include "Station.h"

namespace SmartMet
{
namespace HTTP
{
class Request;
}

namespace Plugin
{
namespace Dali
{
class Config;

class Stations
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::vector<Station> stations;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
