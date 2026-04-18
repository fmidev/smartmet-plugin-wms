// ======================================================================
/*!
 * \brief Unified weather-objects layer backed by dynlib feature detection.
 *
 * Renders a user-specified list of detected meteorological features in
 * a single SVG group. Each entry in the `objects` JSON array selects
 * one detector and its parameters; the layer issues one dynlib call per
 * object, converts the fractional grid-index output into WGS84 lon/lat
 * via the querydata engine's coordinate matrix, projects to screen
 * space, and emits SVG.
 *
 * Supported object types:
 *   "jet_axes"            Wind maxima with zero-shear condition.
 *   "trough_axes"         Vorticity lines (cyclonic shear axes).
 *   "convergence_lines"   Convline detector.
 *   "deformation_lines"   Defline detector.
 *   "vorticity_lines"     Vorline detector (alias target for troughs).
 *
 * Front detection is not exposed by this layer; see
 * smartmet-library-dynlib DESIGN_NOTES for the validation experiment
 * that concluded on-the-fly gradient-based front detection is not
 * chart-quality. Front products should be served from a precomputed
 * external source.
 *
 * JSON example:
 *   {
 *     "layer_type": "weather_objects",
 *     "objects": [
 *       { "type": "jet_axes",   "producer": "ecmwf" },
 *       { "type": "trough_axes" }
 *     ]
 *   }
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include <dynlib/Dynlib.h>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class WeatherObjectsLayer : public Layer
{
 public:
  enum class ObjectKind
  {
    JetAxes,
    TroughAxes,
    ConvergenceLines,
    DeformationLines,
    VorticityLines
  };

  struct ObjectSpec
  {
    ObjectKind kind = ObjectKind::JetAxes;
    std::string u = "WindUMS";
    std::string v = "WindVMS";
    std::string producer;                // empty -> default model
    std::optional<int> level;
    Fmi::Dynlib::LineOptions line_options;
    // Rendering
    std::string css_class;               // appended to the group class
    std::string css_file;                // optional external stylesheet key
  };

  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  std::vector<ObjectSpec> itsObjects;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
