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
      // Check the registry first for OGC-standard tile matrix parameters.
      const TileMatrixSetTemplate* tmpl = nullptr;
      for (const auto& r : REGISTRY)
        if (r.epsg == epsg)
        {
          tmpl = &r;
          break;
        }

      // Parameters filled either from the registry or computed from the EPSG definition.
      double scale0_256 = 0;
      double top_x = 0;
      double top_y = 0;
      unsigned base_w = 1;
      unsigned base_h = 1;
      int max_zoom = 18;
      bool is_geographic = false;
      double bbox_min_x = -180;
      double bbox_min_y = -90;
      double bbox_max_x = 180;
      double bbox_max_y = 90;
      std::string well_known_scale_set_str;

      if (tmpl != nullptr)
      {
        // Use OGC-standard parameters for known projections.
        scale0_256 = tmpl->scale0_at_256;
        top_x = tmpl->top_x;
        top_y = tmpl->top_y;
        base_w = tmpl->base_w;
        base_h = tmpl->base_h;
        max_zoom = tmpl->max_zoom;
        is_geographic = tmpl->is_geographic;
        bbox_min_x = tmpl->bbox_min_x;
        bbox_min_y = tmpl->bbox_min_y;
        bbox_max_x = tmpl->bbox_max_x;
        bbox_max_y = tmpl->bbox_max_y;
        well_known_scale_set_str = tmpl->well_known_scale_set;
      }
      else
      {
        // For EPSGs not in the registry, derive tile matrix parameters from the PROJ database.
        // This allows any EPSG enabled in the WMS configuration to be served as tiles.
        auto epsginfo = Fmi::EPSGInfo::getInfo(epsg);
        if (!epsginfo)
        {
          std::cerr << "WMS/WMTS: EPSG:" << epsg
                    << " is unknown to PROJ, cannot build tile matrix set\n";
          continue;
        }

        const auto& bounds = epsginfo->bounds;
        double width = bounds.east - bounds.west;
        double height = bounds.north - bounds.south;
        if (width <= 0 || height <= 0)
        {
          std::cerr << "WMS/WMTS: EPSG:" << epsg
                    << " has invalid native bounds, cannot build tile matrix set\n";
          continue;
        }

        is_geographic = epsginfo->geodetic;
        top_x = bounds.west;
        top_y = bounds.north;

        // Choose base_w/base_h to cover the full extent with the fewest zoom-0 tiles.
        if (is_geographic)
        {
          base_w = std::max(1u, static_cast<unsigned>(std::round(width / height)));
          base_h = 1u;
        }
        else
        {
          base_w = 1u;
          base_h = std::max(1u, static_cast<unsigned>(std::round(height / width)));
        }

        // Scale denominator at zoom 0 for 256×256 tiles so that base_w tiles span the full width.
        if (is_geographic)
          scale0_256 = (width / base_w) * METERS_PER_DEGREE / (256.0 * OGC_PIXEL_SIZE);
        else
          scale0_256 = (width / base_w) / (256.0 * OGC_PIXEL_SIZE);

        // Limit zoom to where pixel size drops below ~0.5 m.
        max_zoom = std::min(18, static_cast<int>(std::floor(std::log2(scale0_256 / 1785.0))));

        bbox_min_x = epsginfo->bbox.west;
        bbox_min_y = epsginfo->bbox.south;
        bbox_max_x = epsginfo->bbox.east;
        bbox_max_y = epsginfo->bbox.north;
        well_known_scale_set_str = "";
      }

      // Larger tiles → fewer pixels per unit ground area → smaller scale denominator per pixel.
      // This preserves the same geographic coverage per zoom level as the 256×256 reference.
      const double scale_factor = 256.0 / tile_width;
      const double scale0 = scale0_256 * scale_factor;

      TileMatrixSet tms;
      tms.identifier = "EPSG:" + std::to_string(epsg);
      tms.crs = "EPSG:" + std::to_string(epsg);
      tms.well_known_scale_set = well_known_scale_set_str;
      tms.is_geographic = is_geographic;
      tms.bbox_min_x = bbox_min_x;
      tms.bbox_min_y = bbox_min_y;
      tms.bbox_max_x = bbox_max_x;
      tms.bbox_max_y = bbox_max_y;

      for (int zoom = 0; zoom <= max_zoom; ++zoom)
      {
        TileMatrix tm;
        tm.identifier = std::to_string(zoom);
        tm.scale_denominator = scale0 / std::pow(2.0, zoom);
        tm.top_left_corner_x = top_x;
        tm.top_left_corner_y = top_y;
        tm.tile_width = tile_width;
        tm.tile_height = tile_height;
        tm.matrix_width = base_w * static_cast<unsigned>(1u << zoom);
        tm.matrix_height = base_h * static_cast<unsigned>(1u << zoom);
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

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
