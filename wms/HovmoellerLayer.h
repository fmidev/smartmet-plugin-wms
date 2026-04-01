// ======================================================================
/*!
 * \brief Hovmöller diagram layer
 *
 * Renders a 2D time-space cross-section as an SVG grid of coloured
 * rectangles.  Three slice directions are supported:
 *
 *   time_lon   – longitude along X, time along Y, fixed latitude
 *   time_lat   – latitude along X, time along Y, fixed longitude
 *   time_level – pressure level along X, time along Y, fixed lat/lon
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

  // "time_lon" | "time_lat" | "time_level"
  std::string direction = "time_lon";

  // Fixed coordinate for time_lon (latitude) and time_lat (longitude)
  // and for time_level (both latitude and longitude must be set).
  double latitude = 60.0;
  double longitude = 25.0;

  // Spatial range along the varying axis
  double lon_min = 5.0;   // used by time_lon
  double lon_max = 35.0;
  double lat_min = 55.0;  // used by time_lat
  double lat_max = 70.0;

  // Number of evenly-spaced sample points along the space axis.
  // For time_level the number of levels in the data is used instead.
  int nx = 50;

  // Colour bands – same structure as IsobandLayer
  std::vector<Isoband> isobands;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
