// ======================================================================
/*!
 * \brief Weather fronts rendering layer
 *
 * Renders cold, warm, occluded, stationary fronts, troughs and ridges
 * using the SVG textPath + weather-font glyph technique taken from the
 * Frontier rendering engine.
 *
 * Two data sources are supported via the "source" field:
 *
 *   "synthetic"  — curves defined inline in the layer JSON (default)
 *   "grid"       — fronts detected on the fly from querydata using
 *                  Hewson's Thermal Front Parameter (TFP) method
 *
 * See docs/reference.md for full JSON documentation.
 */
// ======================================================================

#pragma once

#include "FrontSource.h"
#include "IsolineFilter.h"
#include "Layer.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class WeatherFrontsLayer : public Layer
{
 public:
  // Exposed so that WeatherFrontsLayer.cpp helper functions can reference it.
  struct FrontStyle
  {
    std::string line_css;
    std::string glyph_css;
    std::string glyph1;
    std::string glyph2;
    double font_size = 30.0;
    double spacing = 60.0;
  };

  // Temporal smoothing of θ across adjacent timesteps. The detected front
  // contours inherit the temporal continuity of the smoothed field, which
  // keeps the fronts from jumping when consecutive frames are animated.
  // Weights are renormalized to sum to 1 after dropping offsets whose
  // corresponding time is not available in the data (e.g. at endpoints).
  struct TemporalSmoothing
  {
    std::vector<int> offsets_minutes;  // e.g. {-180, 0, 180}
    std::vector<double> weights;       // same length as offsets_minutes
  };

  // Configuration for grid-based (TFP) front detection.
  struct GridConfig
  {
    std::string producer;                              // empty = use default producer
    std::string theta_param = "PotentialTemperature";  // θ or T parameter name
    std::string u_param = "WindUMS";                   // U-component parameter name
    std::string v_param = "WindVMS";                   // V-component parameter name
    double level = 850.0;                              // pressure level (hPa)
    double min_gradient = 2e-6;                        // |∇θ| threshold (K/m); below this
                                                       // TFP is masked as missing data
    double min_length_px = 20.0;                       // discard isolines shorter than this
    std::optional<TemporalSmoothing> temporal;         // unset = single-frame detection
  };

  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  void generate_synthetic(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  // Render a single front as a <path> def + <use> stroke + glyph group.
  // If bezierPath is non-empty it is used verbatim as the SVG path "d"
  // attribute; otherwise a straight polyline is built from screenPts.
  // Glyphs are always placed along screenPts so they stay evenly spaced
  // even when the stroke follows a Bezier curve.
  void renderFrontLine(const std::vector<std::pair<double, double>>& screenPts,
                       const std::string& bezierPath,
                       FrontType type,
                       CTPP::CDT& theGlobals,
                       CTPP::CDT& theLayersCdt,
                       State& theState) const;

  const FrontStyle& styleFor(FrontType type) const;

  std::string itsSourceType = "synthetic";
  std::unique_ptr<FrontSource> itsSource;   // non-null when itsSourceType == "synthetic"
  std::optional<GridConfig> itsGridConfig;  // non-empty when itsSourceType == "grid"
  std::map<FrontType, FrontStyle> itsStyles;
  IsolineFilter itsFilter;    // smoother + optional Bezier fitting
  double itsPrecision = 1.0;  // SVG coordinate decimal precision
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
