// ======================================================================
/*!
 * \brief WMTS configuration implementation
 *
 * Provides standard OGC WMTS TileMatrixSets (GoogleMapsCompatible/EPSG:3857
 * and EPSG:4326). Additional sets can be added via config.
 */
// ======================================================================

#include "Config.h"
#include <macgyver/Exception.h>
#include <cmath>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{

namespace
{

// Standard EPSG:3857 (GoogleMapsCompatible) TileMatrixSet, zoom levels 0-18.
// TopLeftCorner: (-20037508.3428, 20037508.3428) in meters.
// Scale at zoom 0: 559082264.0287178.
TileMatrixSet create_epsg3857()
{
  TileMatrixSet tms;
  tms.identifier = "EPSG:3857";
  tms.crs = "EPSG:3857";
  tms.well_known_scale_set = "urn:ogc:def:wkss:OGC:1.0:GoogleMapsCompatible";
  tms.is_geographic = false;
  tms.bbox_min_x = -180.0;
  tms.bbox_min_y = -85.0511;
  tms.bbox_max_x = 180.0;
  tms.bbox_max_y = 85.0511;

  static constexpr double scale0 = 559082264.0287178;
  static constexpr double top_x = -20037508.3428;
  static constexpr double top_y = 20037508.3428;

  for (int zoom = 0; zoom <= 18; ++zoom)
  {
    TileMatrix tm;
    tm.identifier = std::to_string(zoom);
    tm.scale_denominator = scale0 / std::pow(2.0, zoom);
    tm.top_left_corner_x = top_x;
    tm.top_left_corner_y = top_y;
    tm.tile_width = 256;
    tm.tile_height = 256;
    tm.matrix_width = static_cast<unsigned>(1u << zoom);
    tm.matrix_height = static_cast<unsigned>(1u << zoom);
    tms.tile_matrices.push_back(tm);
  }

  return tms;
}

// Standard EPSG:4326 TileMatrixSet, zoom levels 0-12.
// TopLeftCorner: (-180, 90) in degrees.
// Scale at zoom 0: 279541132.0143588 (half of EPSG:3857 zoom 0 to fit 2 tiles across 360°).
TileMatrixSet create_epsg4326()
{
  TileMatrixSet tms;
  tms.identifier = "EPSG:4326";
  tms.crs = "EPSG:4326";
  tms.is_geographic = true;
  tms.bbox_min_x = -180.0;
  tms.bbox_min_y = -90.0;
  tms.bbox_max_x = 180.0;
  tms.bbox_max_y = 90.0;

  static constexpr double scale0 = 279541132.0143588;
  static constexpr double top_x = -180.0;
  static constexpr double top_y = 90.0;

  for (int zoom = 0; zoom <= 12; ++zoom)
  {
    TileMatrix tm;
    tm.identifier = std::to_string(zoom);
    tm.scale_denominator = scale0 / std::pow(2.0, zoom);
    tm.top_left_corner_x = top_x;
    tm.top_left_corner_y = top_y;
    tm.tile_width = 256;
    tm.tile_height = 256;
    // Two tiles wide at zoom 0 to cover 360°, one tile tall to cover 180°
    tm.matrix_width = static_cast<unsigned>(2u << zoom);   // 2^(zoom+1)
    tm.matrix_height = static_cast<unsigned>(1u << zoom);  // 2^zoom
    tms.tile_matrices.push_back(tm);
  }

  return tms;
}

}  // namespace

Config::Config(const Dali::Config& daliConfig, const WMS::Config& wmsConfig)
    : itsDaliConfig(daliConfig), itsWMSConfig(wmsConfig)
{
  init();
}

void Config::init()
{
  try
  {
    // Load standard OGC tile matrix sets. Operators can extend via config in future.
    itsTileMatrixSets.push_back(create_epsg3857());
    itsTileMatrixSets.push_back(create_epsg4326());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WMTS Config::init failed!");
  }
}

const TileMatrixSet* Config::findTileMatrixSet(const std::string& identifier) const
{
  for (const auto& tms : itsTileMatrixSets)
    if (tms.identifier == identifier)
      return &tms;
  return nullptr;
}

const TileMatrix* Config::findTileMatrix(const TileMatrixSet& tms,
                                         const std::string& identifier) const
{
  for (const auto& tm : tms.tile_matrices)
    if (tm.identifier == identifier)
      return &tm;
  return nullptr;
}

bool Config::isValidLayer(const std::string& layer) const
{
  return itsWMSConfig.isValidLayer(layer);
}

bool Config::isValidStyle(const std::string& layer, const std::string& style) const
{
  return itsWMSConfig.isValidStyle(layer, style);
}

bool Config::isValidFormat(const std::string& format) const
{
  return itsSupportedFormats.count(format) > 0;
}

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
