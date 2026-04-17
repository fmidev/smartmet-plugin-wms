// ======================================================================
/*!
 * \brief Weather fronts rendering layer
 *
 * Renders cold, warm, occluded, stationary fronts, troughs and ridges
 * using the SVG textPath + weather-font glyph technique taken from the
 * Frontier rendering engine.
 *
 * Currently the only supported data source is "synthetic" — front
 * curves supplied inline in the layer JSON. The front_source field is
 * retained as an extensibility point for future sources (e.g. WOML).
 *
 * See docs/reference.md for full JSON documentation.
 */
// ======================================================================

#pragma once

#include "FrontSource.h"
#include "Layer.h"
#include <map>
#include <memory>
#include <string>

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

  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  // Render a single front as a <path> def + <use> stroke + glyph group.
  void renderFrontLine(const std::vector<std::pair<double, double>>& screenPts,
                       FrontType type,
                       CTPP::CDT& theGlobals,
                       CTPP::CDT& theLayersCdt,
                       State& theState) const;

  const FrontStyle& styleFor(FrontType type) const;

  std::string itsSourceType = "synthetic";
  std::unique_ptr<FrontSource> itsSource;
  std::map<FrontType, FrontStyle> itsStyles;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
