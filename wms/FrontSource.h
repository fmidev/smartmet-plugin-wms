// ======================================================================
/*!
 * \brief Abstract source of weather front geometry
 *
 * Implementations may read pre-computed curves from WOML files,
 * synthesise them from JSON configuration (SyntheticFrontSource),
 * or derive them on the fly from gridded data (future GridFrontSource).
 *
 * The interface deliberately knows nothing about projections or
 * rendering — it returns only WGS84 geometry.
 */
// ======================================================================

#pragma once

#include "FrontCurve.h"
#include <macgyver/DateTime.h>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class FrontSource
{
 public:
  virtual ~FrontSource() = default;

  // Return all front curves valid at the given time.
  // Implementations are free to ignore validTime if it is not applicable
  // (e.g. SyntheticFrontSource always returns the configured curves).
  virtual std::vector<FrontCurve> getFronts(const Fmi::DateTime& validTime) const = 0;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
