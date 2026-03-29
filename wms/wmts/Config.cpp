// ======================================================================
/*!
 * \brief WMTS configuration implementation
 *
 * Builds TileMatrixSets from the EPSG codes configured in WMS::Config,
 * using the shared registry in TileMatrix.cpp. Tile dimensions are read
 * from the Dali config (wmts.tile_width / wmts.tile_height, default 1024).
 */
// ======================================================================

#include "Config.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{

Config::Config(const Dali::Config& daliConfig, const WMS::Config& wmsConfig)
    : itsDaliConfig(daliConfig), itsWMSConfig(wmsConfig)
{
  init();
}

void Config::init()
{
  try
  {
    // Collect enabled EPSG codes from WMS configuration.
    std::vector<int> epsg_ids;
    for (const auto& [key, ref] : itsWMSConfig.supportedWMSReferences())
    {
      if (!ref.enabled)
        continue;
      if (key.size() > 5 && key.compare(0, 5, "EPSG:") == 0)
      {
        try
        {
          epsg_ids.push_back(std::stoi(key.substr(5)));
        }
        catch (...)
        {
        }
      }
    }

    const unsigned tw = itsDaliConfig.wmtsTileWidth();
    const unsigned th = itsDaliConfig.wmtsTileHeight();
    itsTileMatrixSets = buildTileMatrixSets(epsg_ids, tw, th);
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
