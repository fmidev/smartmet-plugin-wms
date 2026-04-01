// ======================================================================
/*!
 * \brief Weather fronts rendering layer
 *
 * Renders cold, warm, occluded, stationary fronts, troughs and ridges
 * using the SVG textPath + weather-font glyph technique taken from the
 * Frontier rendering engine.
 *
 * The layer is data-source agnostic: a FrontSource implementation is
 * chosen via the "source" JSON field.  Currently supported:
 *   "synthetic"  — curves defined inline in the layer JSON (default)
 *
 * Minimal JSON example:
 *
 *   {
 *     "layer_type": "fronts",
 *     "source": "synthetic",
 *     "fronts": [
 *       { "type": "cold", "side": "left",
 *         "points": [[25,60],[27,62],[30,64]] }
 *     ]
 *   }
 *
 * Per-type style overrides (all optional, defaults match Frontier):
 *
 *   "cold": {
 *     "line_css":  "coldfront",
 *     "glyph_css": "coldfrontglyph",
 *     "glyph1":    "C",
 *     "glyph2":    "c",
 *     "font_size": 30,
 *     "spacing":   60
 *   }
 *
 * The CSS classes must be defined in the product template's <style>
 * block, referencing a weather font that maps the glyph characters to
 * the appropriate symbols.
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
  const FrontStyle& styleFor(FrontType type) const;

  std::unique_ptr<FrontSource> itsSource;
  std::map<FrontType, FrontStyle> itsStyles;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
