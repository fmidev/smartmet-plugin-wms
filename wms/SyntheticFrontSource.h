// ======================================================================
/*!
 * \brief Config-driven front source for testing and development
 *
 * Returns a fixed set of front curves read from the layer JSON at init
 * time.  No engine access, no time dependency — the same curves are
 * returned regardless of validTime.
 *
 * JSON example (inside the layer object):
 *
 *   "fronts": [
 *     { "type": "cold", "side": "left",
 *       "points": [[25.0,60.0],[27.0,62.0],[30.0,64.0]] },
 *     { "type": "warm", "side": "right",
 *       "points": [[20.0,58.0],[23.0,60.0],[26.0,61.5]] }
 *   ]
 */
// ======================================================================

#pragma once

#include "FrontSource.h"
#include <json/json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class SyntheticFrontSource : public FrontSource
{
 public:
  // Parse the "fronts" array from the layer JSON.
  explicit SyntheticFrontSource(const Json::Value& theJson);

  std::vector<FrontCurve> getFronts(const Fmi::DateTime& validTime) const override;

 private:
  std::vector<FrontCurve> itsCurves;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
