// ======================================================================
/*!
 * \brief Map details
 */
// ======================================================================

#pragma once

#include <optional>
#include <engines/gis/MapOptions.h>
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Map
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  bool lines = false;
  Engine::Gis::MapOptions options;

 private:
};  // class Map

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
