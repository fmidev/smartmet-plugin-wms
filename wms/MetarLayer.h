// ======================================================================
/*!
 * \brief MetarLayer - renders METAR aviation weather observations
 *
 * Queries raw TAC METAR messages from the AVI engine and renders a
 * standard meteorological station plot per airport, including:
 *   - Temperature and dewpoint
 *   - Visibility
 *   - Wind barb (direction + speed)
 *   - QNH pressure and wind gust
 *   - Present weather tokens (e.g. -RA, TSRA, +SN)
 *   - Sky condition (cloud layers / CAVOK / SKC / VV)
 *   - Aviation status colour box (LIFR/IFR/MVFR/VFR)
 *
 * All displayed elements are individually configurable via JSON.
 */
// ======================================================================

#pragma once

#ifndef WITHOUT_AVI

#include "Layer.h"
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// ======================================================================
/*!
 * \brief Decoded METAR observation for one station/message
 */
// ======================================================================

struct MetarData
{
  std::string icao;
  double longitude = 0;
  double latitude = 0;

  // Temperature [°C, rounded integer]
  bool has_temperature = false;
  int temperature = 0;

  // Dew point [°C, rounded integer]
  bool has_dewpoint = false;
  int dewpoint = 0;

  // Wind
  bool has_wind = false;
  bool wind_variable = false;  // VRB wind direction
  int wind_direction = 0;      // degrees true, 0-360
  int wind_speed = 0;          // knots

  // Gust [kt]
  bool has_gust = false;
  int wind_gust = 0;

  // Visibility [m, capped at 9999]
  bool has_visibility = false;
  int visibility = 0;

  // Special sky conditions
  bool is_cavok = false;
  bool is_skc = false;  // SKC / CLR / NSC / NCD

  // Present weather tokens as raw TAC text (e.g. "-RA", "TSRA", "+SN")
  std::vector<std::string> weather_tokens;

  // Cloud layers (up to 4)
  struct CloudLayer
  {
    int amount_oktas = 0;  // 1-8 (stored as 2/4/6/8 for FEW/SCT/BKN/OVC)
    int base_hft = 0;      // height in hundreds of feet
    int type = 0;          // 0=none, 1=TCU, 2=CB
  };
  std::vector<CloudLayer> clouds;

  // Vertical visibility [hundreds of feet]; present when VVnnn
  bool has_vertical_vis = false;
  int vertical_vis_hft = 0;

  // QNH pressure [hPa, rounded integer]
  bool has_pressure = false;
  int pressure = 0;
};

// ======================================================================
/*!
 * \brief MetarLayer
 */
// ======================================================================

class MetarLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  // ------------------------------------------------------------------
  // Configuration
  // ------------------------------------------------------------------
  std::string message_type = "METAR";   // METAR, AWSMETAR, SPECI, ...
  std::string message_format = "TAC";
  bool exclude_specis = true;

  int font_size = 10;   // pixels
  int plot_size = 0;    // bounding box side in pixels; 0 = 6*font_size

  bool show_temperature = true;
  bool show_dewpoint = true;
  bool show_visibility = true;
  bool show_status = true;
  bool show_wind = true;
  bool show_pressure = true;
  bool show_gust = true;
  bool show_weather = true;
  bool show_sky_info = true;

  std::string color;  // empty = per-element default colours

  int mindistance = 0;  // minimum pixel spacing between rendered plots

  std::vector<std::string> icaos;      // restrict to these ICAOs (all if empty)
  std::vector<std::string> countries;  // restrict to these ISO2 country codes

  // ------------------------------------------------------------------
  // Helpers
  // ------------------------------------------------------------------

  // Parse a raw TAC METAR/SPECI string into MetarData
  static MetarData parseTAC(const std::string& message,
                             double lon,
                             double lat,
                             const std::string& icao_hint);

  // Format integer temperature/dewpoint: -12→"M12", 3→"03"
  static std::string formatTemp(int t);

  // Format cloud base as 3-digit hundreds-of-feet string
  static std::string formatCloudBase(int base_hft);

  // Cloud amount_oktas → FEW/SCT/BKN/OVC string
  static std::string cloudAmountCode(int amount_oktas);

  // Cloud type code → CB/TCU suffix (empty if none)
  static std::string cloudTypeStr(int type);

  // Build the sky-info multi-line string (CAVOK / SKC / layer list)
  static std::string skyInfoString(const MetarData& d);

  // Build present-weather display string (space-separated TAC tokens)
  static std::string weatherString(const MetarData& d);

  // Aviation status box colour from vis and cloud base
  static std::string statusColor(const MetarData& d);

  // Build SVG path for a wind barb centred at (cx,cy)
  // north_dx/north_dy: unit vector pointing to geographic north in screen coords
  static std::string windBarbPath(int direction_deg,
                                   int speed_kt,
                                   double north_dx,
                                   double north_dy,
                                   double cx,
                                   double cy,
                                   double shaft_len,
                                   double barb_len);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_AVI
