// ======================================================================
/*!
 * \brief WMTS tile bounding box computation
 */
// ======================================================================

#include "TileMatrix.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{

TileBBox computeTileBBox(const TileMatrixSet& tms,
                         const TileMatrix& tm,
                         unsigned tile_row,
                         unsigned tile_col)
{
  try
  {
    // OGC WMTS 1.0.0 §6.1: pixel span in CRS units
    // For projected CRS (meters): pixel_span = ScaleDenominator × 0.00028
    // For geographic CRS (degrees): pixel_span = ScaleDenominator × 0.00028 / METERS_PER_DEGREE
    double pixel_span = tm.scale_denominator * OGC_PIXEL_SIZE;
    if (tms.is_geographic)
      pixel_span /= METERS_PER_DEGREE;

    double tile_span_x = tm.tile_width * pixel_span;
    double tile_span_y = tm.tile_height * pixel_span;

    // TopLeftCorner is the upper-left corner of tile (0,0).
    // Column increases to the right, row increases downward.
    TileBBox bbox;
    bbox.crs = tms.crs;
    bbox.min_x = tm.top_left_corner_x + tile_col * tile_span_x;
    bbox.max_x = bbox.min_x + tile_span_x;
    bbox.max_y = tm.top_left_corner_y - tile_row * tile_span_y;
    bbox.min_y = bbox.max_y - tile_span_y;

    return bbox;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to compute tile bounding box!");
  }
}

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
