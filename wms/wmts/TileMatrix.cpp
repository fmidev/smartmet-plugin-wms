// ======================================================================
/*!
 * \brief WMTS tile bounding box computation and TileMatrixSet registry
 */
// ======================================================================

#include "TileMatrix.h"
#include <gis/EPSGInfo.h>
#include <macgyver/Exception.h>
#include <cmath>
#include <iostream>
#include <optional>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{

namespace
{

struct TileMatrixSetTemplate
{
  int epsg;
  double scale0_at_256;  // scale denominator at zoom 0 for 256×256 tiles
  double top_x;          // TopLeftCorner X in CRS units
  double top_y;          // TopLeftCorner Y in CRS units
  unsigned base_w;       // matrix_width at zoom 0
  unsigned base_h;       // matrix_height at zoom 0
  int max_zoom;
  bool is_geographic;
  double bbox_min_x, bbox_min_y, bbox_max_x, bbox_max_y;  // WGS84 bounding box
  const char* well_known_scale_set;                        // may be empty string
};

// Registry of known EPSG tile matrix sets.
// scale0_at_256 values are per OGC 17-083r4 and WMTS 1.0.0 annex E.
// Bounding boxes are WGS84 (degrees) suitable for capabilities output.
const TileMatrixSetTemplate REGISTRY[] = {
    // EPSG:3857 — Web Mercator (GoogleMapsCompatible / WebMercatorQuad)
    {3857,
     559082264.0287178,
     -20037508.3428,
     20037508.3428,
     1,
     1,
     18,
     false,
     -180.0,
     -85.0511,
     180.0,
     85.0511,
     "urn:ogc:def:wkss:OGC:1.0:GoogleMapsCompatible"},
    // EPSG:4326 — WGS84 geographic (WorldCRS84Quad)
    {4326, 279541132.0143588, -180.0, 90.0, 2, 1, 17, true, -180.0, -90.0, 180.0, 90.0,
     "urn:ogc:def:wkss:OGC:1.0:CRS84"},
    // EPSG:3035 — ETRS89-LAEA Europe (EuropeanETRS89_LAEAQuad)
    {3035, 34906585.0, 2000000.0, 5500000.0, 2, 2, 12, false, -56.5, 24.284, 72.095, 84.735, ""},
    // EPSG:3067 — ETRS-TM35FIN (Finnish national projection)
    {3067, 29257142.857142858, -548576.0, 8388608.0, 1, 1, 13, false, 14.0, 55.0, 35.0, 72.0,
     ""},
};

// The OGC-standard tile matrix parameters of a registered EPSG, or parameters derived from
// the PROJ database for other EPSGs. Returns nothing if the EPSG cannot be used.
std::optional<TileMatrixSetTemplate> tile_matrix_template(int epsg)
{
  // Check the registry first for OGC-standard tile matrix parameters.
  for (const auto& r : REGISTRY)
    if (r.epsg == epsg)
      return r;

  TileMatrixSetTemplate t{};
  t.epsg = epsg;
  t.well_known_scale_set = "";

  // For EPSGs not in the registry, derive tile matrix parameters from the PROJ database.
  // This allows any EPSG enabled in the WMS configuration to be served as tiles.
  auto epsginfo = Fmi::EPSGInfo::getInfo(epsg);
  if (!epsginfo)
  {
    std::cerr << "WMS/WMTS: EPSG:" << epsg << " is unknown to PROJ, cannot build tile matrix set\n";
    return std::nullopt;
  }

  const auto& bounds = epsginfo->bounds;
  double width = bounds.east - bounds.west;
  double height = bounds.north - bounds.south;
  if (width <= 0 || height <= 0)
  {
    std::cerr << "WMS/WMTS: EPSG:" << epsg
              << " has invalid native bounds, cannot build tile matrix set\n";
    return std::nullopt;
  }

  t.is_geographic = epsginfo->geodetic;
  t.top_x = bounds.west;
  t.top_y = bounds.north;

  // Choose base_w/base_h to cover the full extent with the fewest zoom-0 tiles.
  if (t.is_geographic)
  {
    t.base_w = std::max(1U, static_cast<unsigned>(std::round(width / height)));
    t.base_h = 1U;
  }
  else
  {
    t.base_w = 1U;
    t.base_h = std::max(1U, static_cast<unsigned>(std::round(height / width)));
  }

  // Scale denominator at zoom 0 for 256×256 tiles so that base_w tiles span the full width.
  if (t.is_geographic)
    t.scale0_at_256 = (width / t.base_w) * METERS_PER_DEGREE / (256.0 * OGC_PIXEL_SIZE);
  else
    t.scale0_at_256 = (width / t.base_w) / (256.0 * OGC_PIXEL_SIZE);

  // Limit zoom to where pixel size drops below ~0.5 m.
  t.max_zoom = std::min(18, static_cast<int>(std::floor(std::log2(t.scale0_at_256 / 1785.0))));

  t.bbox_min_x = epsginfo->bbox.west;
  t.bbox_min_y = epsginfo->bbox.south;
  t.bbox_max_x = epsginfo->bbox.east;
  t.bbox_max_y = epsginfo->bbox.north;
  return t;
}

}  // namespace

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

std::vector<TileMatrixSet> buildTileMatrixSets(const std::vector<int>& epsg_ids,
                                               unsigned tile_width,
                                               unsigned tile_height)
{
  try
  {
    std::vector<TileMatrixSet> result;

    for (int epsg : epsg_ids)
    {
      const auto tmpl = tile_matrix_template(epsg);
      if (!tmpl)
        continue;

      const double scale0_256 = tmpl->scale0_at_256;
      const double top_x = tmpl->top_x;
      const double top_y = tmpl->top_y;
      const unsigned base_w = tmpl->base_w;
      const unsigned base_h = tmpl->base_h;
      const int max_zoom = tmpl->max_zoom;

      // Larger tiles → fewer pixels per unit ground area → smaller scale denominator per pixel.
      // This preserves the same geographic coverage per zoom level as the 256×256 reference.
      const double scale_factor = 256.0 / tile_width;
      const double scale0 = scale0_256 * scale_factor;

      TileMatrixSet tms;
      tms.identifier = "EPSG:" + std::to_string(epsg);
      tms.crs = "EPSG:" + std::to_string(epsg);
      tms.well_known_scale_set = tmpl->well_known_scale_set;
      tms.is_geographic = tmpl->is_geographic;
      tms.bbox_min_x = tmpl->bbox_min_x;
      tms.bbox_min_y = tmpl->bbox_min_y;
      tms.bbox_max_x = tmpl->bbox_max_x;
      tms.bbox_max_y = tmpl->bbox_max_y;

      for (int zoom = 0; zoom <= max_zoom; ++zoom)
      {
        TileMatrix tm;
        tm.identifier = std::to_string(zoom);
        tm.scale_denominator = scale0 / std::pow(2.0, zoom);
        tm.top_left_corner_x = top_x;
        tm.top_left_corner_y = top_y;
        tm.tile_width = tile_width;
        tm.tile_height = tile_height;
        tm.matrix_width = base_w * static_cast<unsigned>(1U << zoom);
        tm.matrix_height = base_h * static_cast<unsigned>(1U << zoom);
        tms.tile_matrices.push_back(tm);
      }

      result.push_back(std::move(tms));
    }

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "buildTileMatrixSets failed!");
  }
}

// ----------------------------------------------------------------------

int webMercatorZoom(const TileMatrixSet& tms, const TileMatrix& tm)
{
  if (tms.crs.find("3857") == std::string::npos && tms.crs.find("900913") == std::string::npos)
    return -1;

  const double half = 20037508.342789244;
  if (std::abs(tm.top_left_corner_x + half) > 1 || std::abs(tm.top_left_corner_y - half) > 1)
    return -1;

  if (tm.matrix_width != tm.matrix_height)
    return -1;
  for (int z = 0; z <= 30; z++)
    if (tm.matrix_width == (1U << z))
      return z;
  return -1;
}

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
