// ======================================================================
/*!
 * \brief Mapbox Vector Tile (MVT) encoder
 *
 * Builds a protobuf-encoded MVT tile from OGR geometry objects.
 *
 * Usage:
 *   MVTTileBuilder tile(xmin, ymin, xmax, ymax);
 *   auto& lyr = tile.layer("temperature");
 *   lyr.addFeature(*geom, {{"value", 12.5}, {"class", std::string("warm")}});
 *   std::string bytes = tile.serialize();
 */
// ======================================================================

#pragma once

#include "vector_tile.pb.h"
#include <ogr_geometry.h>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// Attribute value: string, double, int64, or bool
using MVTValue = std::variant<std::string, double, int64_t, bool>;

// ----------------------------------------------------------------------
/*!
 * \brief Builds features for a single MVT layer.
 *
 * Obtained via MVTTileBuilder::layer(name).  Not copyable; the reference
 * stays valid as long as the parent MVTTileBuilder is alive.
 */
// ----------------------------------------------------------------------

class MVTLayerBuilder
{
 public:
  // Construct from a tile Layer proto message and the tile's geographic bbox
  MVTLayerBuilder(vector_tile::Tile_Layer* layer,
                  double xmin,
                  double ymin,
                  double xmax,
                  double ymax,
                  unsigned extent);

  MVTLayerBuilder(const MVTLayerBuilder&) = delete;
  MVTLayerBuilder& operator=(const MVTLayerBuilder&) = delete;
  MVTLayerBuilder(MVTLayerBuilder&&) = default;

  /**
   * Add one feature from an OGR geometry with optional attributes.
   *
   * Supported geometry types: Point, MultiPoint, LineString, MultiLineString,
   * Polygon, MultiPolygon, GeometryCollection (sub-geometries added individually).
   * Empty geometries are silently skipped.
   */
  void addFeature(const OGRGeometry& geom,
                  const std::vector<std::pair<std::string, MVTValue>>& attrs = {});

 private:
  vector_tile::Tile_Layer* itsLayer;  // non-owning
  double itsXMin, itsYMin, itsXMax, itsYMax;
  unsigned itsExtent;

  // Key and value intern tables (deduplicate entries in the layer)
  std::unordered_map<std::string, uint32_t> itsKeyIndex;
  // Value de-dup is per-serialized-proto-string; simpler to just append
  // (values are often unique floats/ints anyway)

  std::pair<int32_t, int32_t> project(double x, double y) const;

  std::vector<uint32_t> encodeGeometry(const OGRGeometry& geom) const;
  void encodeRing(const OGRLinearRing* ring,
                  bool close,
                  int32_t& cx,
                  int32_t& cy,
                  std::vector<uint32_t>& out) const;
  void encodeLineString(const OGRLineString* ls,
                        bool close,
                        int32_t& cx,
                        int32_t& cy,
                        std::vector<uint32_t>& out) const;

  vector_tile::Tile_GeomType ogrTypeToMVT(const OGRGeometry& geom) const;

  uint32_t internKey(const std::string& key);
  uint32_t appendValue(const MVTValue& val);
};

// ----------------------------------------------------------------------
/*!
 * \brief Builds a complete MVT Tile from multiple named layers.
 */
// ----------------------------------------------------------------------

class MVTTileBuilder
{
 public:
  /**
   * @param xmin,ymin,xmax,ymax  Geographic bbox of the tile in the map CRS
   * @param extent                Tile coordinate range (default 4096)
   */
  MVTTileBuilder(double xmin, double ymin, double xmax, double ymax, unsigned extent = 4096);

  MVTTileBuilder(const MVTTileBuilder&) = delete;
  MVTTileBuilder& operator=(const MVTTileBuilder&) = delete;

  /**
   * Return a layer builder for the given name, creating the layer if needed.
   * The returned reference is valid for the lifetime of this MVTTileBuilder.
   */
  MVTLayerBuilder& layer(const std::string& name);

  /** True if no layers have been added yet. */
  bool empty() const { return itsBuilders.empty(); }

  /** Serialize to binary protobuf. */
  std::string serialize() const;

 private:
  vector_tile::Tile itsTile;
  double itsXMin, itsYMin, itsXMax, itsYMax;
  unsigned itsExtent;

  // Layer builders in order of creation (stable references)
  std::vector<MVTLayerBuilder> itsBuilders;
  std::unordered_map<std::string, size_t> itsLayerIndex;  // name → index in itsBuilders
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
