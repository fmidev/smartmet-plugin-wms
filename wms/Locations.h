// ======================================================================
/*!
 * \brief Locations
 */
// ======================================================================

#pragma once

#include "Location.h"

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
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::vector<Location> locations;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
