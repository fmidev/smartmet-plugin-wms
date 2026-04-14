// ======================================================================
/*!
 * \brief OpenStreetMap layer
 *
 * Renders OSM data loaded into PostGIS via osm2pgsql. Supports polygons
 * (landuse, water, buildings), linear features (roads, coastlines,
 * administrative boundaries) and place labels. ETag is based on the
 * modification time of a configurable timestamp file that should be
 * touched after each osm2pgsql import.
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

  // For OGC Tiles MVT output: if this layer uses the pmtiles backend, return
  // the raw (possibly compressed) tile bytes directly from the mmap'd file.
  // Returns an empty optional for the postgis backend or if the tile is missing.
  std::optional<Engine::OSM::TileData> getRawMVTTile(uint8_t z,
                                                      uint32_t x,
                                                      uint32_t y,
                                                      const State& theState) const;

  // Layer interface: for pmtiles backend, returns the raw MVT protobuf bytes
  // from the mmap'd PMTiles file using tile coordinates stored in State.
  // Returns std::nullopt for the postgis backend or if no tile coords are set.
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

  // ---- PMTiles backend (alternative to PostGIS feature_sets) ----
  // When backend == "pmtiles", addMVTLayer() returns raw tile bytes from the
  // OSM engine, and generate() falls back to decoding them via OGR.
  // When backend == "postgis" (default), feature_sets are used.
  std::string itsBackend = "postgis";      // "postgis" | "pmtiles"
  std::string itsOSMSource;               // source name in OSM engine config

  // For postgis backend: touched after each osm2pgsql import
  std::filesystem::path itsTimestampFile;
  double precision = 1.0;

  std::int64_t getDataTimestamp(const State& theState) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OSM
