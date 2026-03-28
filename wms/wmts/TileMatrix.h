// ======================================================================
/*!
 * \brief WMTS TileMatrix and TileMatrixSet data structures
 *
 * Implements OGC WMTS 1.0.0 (OGC 07-057r7) tile coordinate geometry.
 */
// ======================================================================

#pragma once

#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{

// OGC standard: 0.28 mm per pixel at scale denominator 1
static constexpr double OGC_PIXEL_SIZE = 0.00028;

// WGS84 meters per degree at the equator (used for geographic CRS pixel size)
static constexpr double METERS_PER_DEGREE = 111319.49079327357;

struct TileMatrix
{
  std::string identifier;     // zoom level id, e.g. "0", "1", ..., "18"
  double scale_denominator;   // OGC scale denominator
  double top_left_corner_x;   // in CRS units
  double top_left_corner_y;
  unsigned tile_width = 256;
  unsigned tile_height = 256;
  unsigned matrix_width = 1;   // number of tiles horizontally at this zoom level
  unsigned matrix_height = 1;  // number of tiles vertically
};

struct TileMatrixSet
{
  std::string identifier;           // e.g. "EPSG:3857" or "GoogleMapsCompatible"
  std::string crs;                  // CRS identifier (EPSG code or OGC URN)
  std::string well_known_scale_set;  // optional OGC well-known scale set URI
  bool is_geographic = false;       // true for degree-based CRS (EPSG:4326)
  std::vector<TileMatrix> tile_matrices;  // ordered from coarsest (zoom 0) to finest

  // WGS84 bounding box of this tile matrix set (for capabilities)
  double bbox_min_x = -180;
  double bbox_min_y = -90;
  double bbox_max_x = 180;
  double bbox_max_y = 90;
};

struct TileBBox
{
  double min_x;
  double min_y;
  double max_x;
  double max_y;
  std::string crs;
};

// Compute the geographic bounding box of a single tile.
// tile_row and tile_col are zero-based, row increases downward (north to south).
TileBBox computeTileBBox(const TileMatrixSet& tms,
                         const TileMatrix& tm,
                         unsigned tile_row,
                         unsigned tile_col);

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
