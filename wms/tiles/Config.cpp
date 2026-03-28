// ======================================================================
/*!
 * \brief OGC API - Tiles configuration implementation
 *
 * Provides standard OGC TileMatrixSets (WebMercatorQuad/EPSG:3857 and
 * WorldCRS84Quad/EPSG:4326). Delegates collection (layer) validation to
 * WMS::Config.
 */
// ======================================================================

#include "Config.h"
#include <macgyver/Exception.h>
#include <cmath>

namespace SmartMet
{
namespace Plugin
{
namespace Tiles
{

namespace
{

// Standard EPSG:3857 (WebMercatorQuad) TileMatrixSet, zoom levels 0-18.
WMTS::TileMatrixSet create_epsg3857()
{
  WMTS::TileMatrixSet tms;
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
    WMTS::TileMatrix tm;
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

// Standard EPSG:4326 (WorldCRS84Quad) TileMatrixSet, zoom levels 0-12.
WMTS::TileMatrixSet create_epsg4326()
{
  WMTS::TileMatrixSet tms;
  tms.identifier = "EPSG:4326";
  tms.crs = "EPSG:4326";
  tms.well_known_scale_set = "urn:ogc:def:wkss:OGC:1.0:CRS84";
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
    WMTS::TileMatrix tm;
    tm.identifier = std::to_string(zoom);
    tm.scale_denominator = scale0 / std::pow(2.0, zoom);
    tm.top_left_corner_x = top_x;
    tm.top_left_corner_y = top_y;
    tm.tile_width = 256;
    tm.tile_height = 256;
    // Two tiles wide at zoom 0, one tile tall
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
    itsTileMatrixSets.push_back(create_epsg3857());
    itsTileMatrixSets.push_back(create_epsg4326());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Tiles Config::init failed!");
  }
}

const WMTS::TileMatrixSet* Config::findTileMatrixSet(const std::string& identifier) const
{
  for (const auto& tms : itsTileMatrixSets)
    if (tms.identifier == identifier)
      return &tms;
  return nullptr;
}

const WMTS::TileMatrix* Config::findTileMatrix(const WMTS::TileMatrixSet& tms,
                                                const std::string& identifier) const
{
  for (const auto& tm : tms.tile_matrices)
    if (tm.identifier == identifier)
      return &tm;
  return nullptr;
}

bool Config::isValidCollection(const std::string& id) const
{
  return itsWMSConfig.isValidLayer(id);
}

bool Config::isValidStyle(const std::string& collection, const std::string& style) const
{
  return itsWMSConfig.isValidStyle(collection, style);
}

bool Config::isValidFormat(const std::string& format) const
{
  return itsSupportedFormats.count(format) > 0;
}

std::string Config::crsUri(const std::string& crs)
{
  if (crs == "EPSG:3857")
    return "http://www.opengis.net/def/crs/EPSG/0/3857";
  if (crs == "EPSG:4326")
    return "http://www.opengis.net/def/crs/EPSG/0/4326";
  // Generic fallback: split on ':' and build URI
  auto colon = crs.find(':');
  if (colon != std::string::npos)
    return "http://www.opengis.net/def/crs/" + crs.substr(0, colon) + "/0/" +
           crs.substr(colon + 1);
  return crs;
}

std::string Config::wellKnownScaleSetUri(const std::string& wkss_urn)
{
  // "urn:ogc:def:wkss:OGC:1.0:GoogleMapsCompatible"
  //   → "http://www.opengis.net/def/wkss/OGC/1.0/GoogleMapsCompatible"
  const std::string prefix = "urn:ogc:def:wkss:";
  if (wkss_urn.compare(0, prefix.size(), prefix) == 0)
  {
    std::string rest = wkss_urn.substr(prefix.size());
    for (char& c : rest)
      if (c == ':')
        c = '/';
    return "http://www.opengis.net/def/wkss/" + rest;
  }
  return wkss_urn;
}

std::string Config::tmsDefinitionUri(const std::string& identifier)
{
  if (identifier == "EPSG:3857")
    return "http://www.opengis.net/def/tilematrixset/OGC/1.0/WebMercatorQuad";
  if (identifier == "EPSG:4326")
    return "http://www.opengis.net/def/tilematrixset/OGC/1.0/WorldCRS84Quad";
  return {};
}

}  // namespace Tiles
}  // namespace Plugin
}  // namespace SmartMet
