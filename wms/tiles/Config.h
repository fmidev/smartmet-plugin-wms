// ======================================================================
/*!
 * \brief OGC API - Tiles configuration
 *
 * Holds TileMatrixSet definitions and delegates collection/layer queries
 * to WMS::Config. Shares the product file tree with WMS.
 *
 * References:
 *   OGC API - Tiles 1.0 (OGC 20-057)
 *   OGC Two Dimensional Tile Matrix Set 2.0 (OGC 17-083r4)
 */
// ======================================================================

#pragma once

#include "../wmts/TileMatrix.h"
#include "../Config.h"
#include "../wms/Config.h"
#include <set>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Tiles
{

class Config
{
 public:
  Config(const Dali::Config& daliConfig, const WMS::Config& wmsConfig);
  ~Config() = default;
  Config() = delete;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;
  Config(Config&&) = delete;
  Config& operator=(Config&&) = delete;

  const WMS::Config& wmsConfig() const { return itsWMSConfig; }
  const Dali::Config& getDaliConfig() const { return itsDaliConfig; }

  const std::vector<WMTS::TileMatrixSet>& tileMatrixSets() const { return itsTileMatrixSets; }
  const std::set<std::string>& supportedFormats() const { return itsSupportedFormats; }

  const WMTS::TileMatrixSet* findTileMatrixSet(const std::string& identifier) const;
  const WMTS::TileMatrix* findTileMatrix(const WMTS::TileMatrixSet& tms,
                                          const std::string& identifier) const;

  // Collection == WMS layer
  bool isValidCollection(const std::string& id) const;
  bool isValidStyle(const std::string& collection, const std::string& style) const;
  bool isValidFormat(const std::string& format) const;

  // Convert CRS identifier to OGC HTTP URI
  // e.g. "EPSG:3857" → "http://www.opengis.net/def/crs/EPSG/0/3857"
  static std::string crsUri(const std::string& crs);

  // Convert OGC well-known scale set URN to HTTP URI
  // e.g. "urn:ogc:def:wkss:OGC:1.0:GoogleMapsCompatible"
  //   → "http://www.opengis.net/def/wkss/OGC/1.0/GoogleMapsCompatible"
  static std::string wellKnownScaleSetUri(const std::string& wkss_urn);

  // Standard OGC TileMatrixSet definition URI for a known TMS identifier
  static std::string tmsDefinitionUri(const std::string& identifier);

 private:
  void init();

  const Dali::Config& itsDaliConfig;
  const WMS::Config& itsWMSConfig;

  std::vector<WMTS::TileMatrixSet> itsTileMatrixSets;
  std::set<std::string> itsSupportedFormats{"image/png", "image/webp", "image/svg+xml"};
};

}  // namespace Tiles
}  // namespace Plugin
}  // namespace SmartMet
