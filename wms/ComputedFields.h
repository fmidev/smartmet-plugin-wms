// ======================================================================
/*!
 * \brief On-the-fly derived scalar fields for isobanding/isolining.
 *
 * Currently provides:
 *   - smoothScalar: separable 1-2-1 binomial smoother (missing-value aware)
 *   - computeTFP:   Hewson's Thermal Front Parameter on any scalar field
 *
 * TFP is a generic "ridge of gradient magnitude" detector; it is not
 * specific to temperature. Useful underlying fields include θ, θw, θe,
 * specific humidity, wind speed, and potential vorticity.
 *
 * References:
 *   Hewson (1998), "Objective fronts". Meteorol. Appl. 5, 37-65.
 *   Schemm, Sprenger, Wernli (2018), MWR 146, 2119-2137.
 *   Spensberger et al., dynlib (U. Bergen) — most complete open
 *   implementation, recommended starting point if objective front
 *   detection (not just TFP diagnostic display) is ever needed.
 */
// ======================================================================

#pragma once

#include <gis/Types.h>
#include <newbase/NFmiDataMatrix.h>
#include <json/json.h>
#include <string>

namespace Fmi
{
class CoordinateMatrix;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace ComputedFields
{

// Config for the TFP metaparameter. Carried on the layer's JSON as
// a "tfp" block; see parseTfpOptions().
struct TfpOptions
{
  std::string field = "PotentialTemperature";  // underlying scalar
  int smoothing_passes = 3;                    // 1-2-1 binomial passes before TFP
  double min_gradient = 0.0;                   // mask TFP where |∇field| < this (0 = no mask)
  double scale = 1.0;                          // multiply TFP result by this factor
                                               // before contouring; use to bring different
                                               // underlying fields onto a shared colour scale
                                               // (e.g. 1e10 for θ, 1e9 for humidity)
};

// Conventional metaparameter name recognised by the isoband/isoline layers.
constexpr const char* kTfpName = "TFP";

// True if the layer's "parameter" string selects TFP.
bool isTfpParameter(const std::string& parameter);

// Parse a "tfp" JSON object into TfpOptions. Consumes known keys.
TfpOptions parseTfpOptions(Json::Value& tfpJson);

// Contribute to the layer's cache hash so that different TFP configs
// don't collide in the isoband/isoline caches.
void hashTfpOptions(std::size_t& seed, const TfpOptions& opts);

// Smooth a scalar matrix with N separable 1-2-1 binomial passes. Each
// pass is equivalent to a (1,2,1)/(2,4,2)/(1,2,1)/16 2D kernel and
// grows the effective Gaussian σ as √N cells. Missing neighbours
// collapse the kernel onto the present weights. Returns a new matrix
// of the same shape.
NFmiDataMatrix<float> smoothScalar(const NFmiDataMatrix<float>& field, int passes);

// Compute Hewson's TFP on a scalar matrix:
//
//     TFP = -∇(|∇F|) · ∇F / |∇F|
//
// coords must be WGS84 lon/lat degrees (obtain via
// qEngine.getWorldCoordinates(q, Fmi::SpatialReference("WGS84"))).
// Metric gradients are derived using the spherical Earth
// approximation. Cells where |∇F| < min_gradient are masked to
// kFloatMissing in the returned TFP matrix; pass 0.0 to disable.
// Returns a matrix of the same shape as field.
NFmiDataMatrix<float> computeTFP(const NFmiDataMatrix<float>& field,
                                 const Fmi::CoordinateMatrix& coordsWgs84,
                                 double min_gradient);

}  // namespace ComputedFields
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
