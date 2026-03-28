// ======================================================================
/*!
 * \brief WMTS configuration
 *
 * Holds TileMatrixSet definitions and delegates layer queries to WMS::Config.
 * WMTS uses the same product file tree and layer registry as WMS.
 */
// ======================================================================

#pragma once

#include "TileMatrix.h"
#include "../Config.h"
#include "../wms/Config.h"
#include <string>
#include <set>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
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

  const std::vector<TileMatrixSet>& tileMatrixSets() const { return itsTileMatrixSets; }
  const std::set<std::string>& supportedFormats() const { return itsSupportedFormats; }

  const TileMatrixSet* findTileMatrixSet(const std::string& identifier) const;
  const TileMatrix* findTileMatrix(const TileMatrixSet& tms,
                                   const std::string& identifier) const;

  // Layer validation delegates to WMS::Config
  bool isValidLayer(const std::string& layer) const;
  bool isValidStyle(const std::string& layer, const std::string& style) const;
  bool isValidFormat(const std::string& format) const;

 private:
  void init();

  const Dali::Config& itsDaliConfig;
  const WMS::Config& itsWMSConfig;

  std::vector<TileMatrixSet> itsTileMatrixSets;
  std::set<std::string> itsSupportedFormats{"image/png", "image/webp", "image/svg+xml"};
};

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
