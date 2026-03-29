// ======================================================================
/*!
 * \brief OGC API - Tiles configuration implementation
 *
 * Builds TileMatrixSets from the EPSG codes configured in WMS::Config,
 * using the shared registry in wmts/TileMatrix.cpp. Tile dimensions are
 * read from the Dali config (wmts.tile_width / wmts.tile_height, default 1024).
 */
// ======================================================================

#include "Config.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Tiles
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
    itsTileMatrixSets = WMTS::buildTileMatrixSets(epsg_ids, tw, th);
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
