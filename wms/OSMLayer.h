// ======================================================================
/*!
 * \brief OpenStreetMap layer
 *
 * Two data sources are supported:
 *
 *  1. Vector tile datasets of the OSM engine ("dataset"), rendered by the
 *     engine with a MapLibre style ("style"). This is the recommended way:
 *     the data is a directory of PMTiles files such as the Protomaps builds,
 *     and the resolution adapts to what is available in each region.
 *
 *  2. OSM data loaded into PostGIS via osm2pgsql ("feature_sets"). ETag is
 *     based on the modification time of a configurable timestamp file that
 *     should be touched after each osm2pgsql import.
 *
 * Output: SVG (raster products) and Mapbox Vector Tiles (OGC Tiles).
 */
// ======================================================================

#pragma once

#ifndef WITHOUT_OSM

#include "Attributes.h"
#include "Layer.h"
#include <engines/gis/MapOptions.h>
#include <engines/osm/Engine.h>
#include <gis/Types.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class OSMLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;
  void addMVTLayer(class MVTTileBuilder& builder, State& theState) override;

  // Layer interface: for vector tile datasets, returns the MVT tile at the
  // WebMercatorQuad tile coordinates stored in State. Returns std::nullopt for
  // the PostGIS backend or if no tile coordinates are set.
  std::optional<std::string> getRawMVTBytes(const State& theState) const override;

  std::size_t hash_value(const State& theState) const override;

 private:
  struct FeatureSet
  {
    Engine::Gis::MapOptions mapOptions;
    bool lines = false;   // true → lineclip; false → polyclip
    bool labels = false;  // render name attribute as SVG <text>
    Attributes attributes;
    std::optional<std::string> css;
    std::string mvt_layer_name;  // layer name in MVT output (defaults to table name)

    std::size_t hash_value() const;
  };

  std::vector<FeatureSet> itsFeatureSets;

  // ---- Vector tile datasets (alternative to PostGIS feature_sets) ----
  bool isDataset() const { return !itsOptions.dataset.empty(); }
  void generateDataset(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  void parseOverrides(const Json::Value& theJson);

  Engine::OSM::MapOptions itsOptions;  // dataset, style and rendering options
  bool itsAttribution = false;         // draw the data attribution into the image
  // SVG attributes replacing those of the rendered lines and polygon fills
  std::vector<std::pair<std::string, std::string>> itsOverrides;

  // For postgis backend: touched after each osm2pgsql import
  std::filesystem::path itsTimestampFile;
  double precision = 1.0;

  std::int64_t getDataTimestamp(const State& theState) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OSM
