// ======================================================================
/*!
 * \brief Hovmöller diagram layer
 *
 * Renders a 2D time-space cross-section (or vertical cross-section) as
 * an SVG grid of coloured rectangles.  Five directions are supported:
 *
 *   time_lon   – longitude along X, time along Y, fixed latitude
 *   time_lat   – latitude along X, time along Y, fixed longitude
 *   time_level – pressure level along X, time along Y, fixed lat/lon
 *   lon_level  – longitude along X, level along Y, fixed lat + time
 *   lat_level  – latitude along X,  level along Y, fixed lon + time
 *
 * The last two are vertical cross-sections (not strictly Hovmöller
 * diagrams) but share the same rendering infrastructure.
 *
 * Colours are assigned via the same Isoband definitions used by
 * IsobandLayer, so any existing isoband JSON + CSS can be reused.
 */
// ======================================================================

#pragma once

#include "Isoband.h"
#include "Layer.h"
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class HovmoellerLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals,
                CTPP::CDT& theLayersCdt,
                State& theState) override;

  void getFeatureInfo(CTPP::CDT& /*theInfo*/, const State& /*theState*/) override {}

  // ---- slice configuration ----

  // "time_lon" | "time_lat" | "time_level" | "lon_level" | "lat_level"
  std::string direction = "time_lon";

  // Fixed coordinate for time_lon (latitude) and time_lat (longitude)
  // and for time_level / lon_level / lat_level (both lat and lon).
  double latitude = 60.0;
  double longitude = 25.0;

  // Spatial range along the varying axis
  double lon_min = 5.0;   // used by time_lon and lon_level
  double lon_max = 35.0;
  double lat_min = 55.0;  // used by time_lat and lat_level
  double lat_max = 70.0;

  // Number of evenly-spaced sample points along the space axis.
  // For *_level directions the number of levels in the data is used.
  int nx = 50;

  // Time axis direction (Hovmöller directions only).
  // true  = oldest time at top, time increases downward (classic convention).
  // false = newest time at top, time increases upward.
  bool time_ascending = true;

  // Level axis direction (time_level, lon_level, lat_level).
  // false = surface (high pressure) on left / top (met convention).
  // true  = surface on right / bottom.
  bool level_ascending = false;

  // Colour bands – same structure as IsobandLayer
  std::vector<Isoband> isobands;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
