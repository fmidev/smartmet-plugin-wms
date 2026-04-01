// ======================================================================
/*!
 * \brief Weather front curve data structures
 *
 * A FrontCurve is the unit of data exchanged between a FrontSource
 * and WeatherFrontsLayer.  It carries only the geometry and enough
 * metadata for the renderer to pick the right glyph style — there is
 * no dependency on any particular data format (WOML, grid, etc.).
 */
// ======================================================================

#pragma once

#include <utility>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

enum class FrontType
{
  Cold,
  Warm,
  Occluded,
  Stationary,
  Trough,
  Ridge
};

// Which side of the directed line the symbols face.
// "Left" means the symbol protrudes to the left when walking the
// point sequence in order; "Right" protrudes to the right.
enum class FrontSide
{
  Left,
  Right
};

struct FrontCurve
{
  FrontType type = FrontType::Cold;
  FrontSide side = FrontSide::Left;
  // Control points in WGS84 lon/lat degrees, in order along the front.
  std::vector<std::pair<double, double>> points;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
