// ======================================================================
/*!
 * \brief Several observation parameters combined into one layer
 *
 * The "parameter" setting of a symbol or number layer may be a list
 * instead of a single parameter name:
 *
 *   "parameter": [
 *      "WW",
 *      { "parameter": "WaWa", "offset": 100 },
 *      { "parameter": "roadww", "producer": "road", "keyword": "road_weather_stations_master" }
 *   ]
 *
 * Each element is either a parameter name or an object with the settings
 * parameter (required), producer, keyword, multiplier and offset. The
 * sources are read in the listed order, and for each station the first
 * valid value wins.
 */
// ======================================================================

#pragma once

#include <spine/Json.h>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct ObservationSource
{
  std::string parameter;
  std::optional<std::string> producer;
  std::optional<std::string> keyword;
  double multiplier = 1.0;
  double offset = 0.0;

  std::size_t hash_value() const;
};

struct ObservationSources
{
  // Extract "parameter" from the JSON if it is a list. Must be called before
  // Properties::init, which accepts only a single parameter name.
  void init(Json::Value& theJson);

  bool empty() const { return sources.empty(); }
  std::size_t hash_value() const;

  std::vector<ObservationSource> sources;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
