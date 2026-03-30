// ======================================================================

#include "MapboxVectorTile.h"
#include <macgyver/Exception.h>
#include <ogr_geometry.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{

// MVT command IDs
constexpr uint32_t CMD_MOVETO    = 1;
constexpr uint32_t CMD_LINETO    = 2;
constexpr uint32_t CMD_CLOSEPATH = 7;

uint32_t makeCmd(uint32_t id, uint32_t count)
{
  return (id << 3) | count;
}

// ZigZag encode a signed delta for protobuf varint encoding
uint32_t zigzag(int32_t v)
{
  return static_cast<uint32_t>((v << 1) ^ (v >> 31));
}

}  // namespace

// ======================================================================
// MVTLayerBuilder
// ======================================================================

MVTLayerBuilder::MVTLayerBuilder(vector_tile::Tile_Layer* layer,
                                 double xmin,
                                 double ymin,
                                 double xmax,
                                 double ymax,
                                 unsigned extent)
    : itsLayer(layer),
      itsXMin(xmin),
      itsYMin(ymin),
      itsXMax(xmax),
      itsYMax(ymax),
      itsExtent(extent)
{
  itsLayer->set_version(2);
  itsLayer->set_extent(extent);
}

// ----------------------------------------------------------------------

std::pair<int32_t, int32_t> MVTLayerBuilder::project(double x, double y) const
{
  const double xsize = itsXMax - itsXMin;
  const double ysize = itsYMax - itsYMin;
  if (xsize == 0 || ysize == 0)
    return {0, 0};

  const auto tx = static_cast<int32_t>(std::round((x - itsXMin) / xsize * itsExtent));
  const auto ty = static_cast<int32_t>(std::round((itsYMax - y) / ysize * itsExtent));
  return {tx, ty};
}

// ----------------------------------------------------------------------

uint32_t MVTLayerBuilder::internKey(const std::string& key)
{
  auto it = itsKeyIndex.find(key);
  if (it != itsKeyIndex.end())
    return it->second;
  const auto idx = static_cast<uint32_t>(itsLayer->keys_size());
  itsLayer->add_keys(key);
  itsKeyIndex[key] = idx;
  return idx;
}

// ----------------------------------------------------------------------

uint32_t MVTLayerBuilder::appendValue(const MVTValue& val)
{
  const auto idx = static_cast<uint32_t>(itsLayer->values_size());
  auto* v = itsLayer->add_values();
  std::visit(
      [v](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>)
          v->set_string_value(arg);
        else if constexpr (std::is_same_v<T, double>)
          v->set_double_value(arg);
        else if constexpr (std::is_same_v<T, int64_t>)
          v->set_int_value(arg);
        else if constexpr (std::is_same_v<T, bool>)
          v->set_bool_value(arg);
      },
      val);
  return idx;
}

// ----------------------------------------------------------------------

void MVTLayerBuilder::encodeRing(const OGRLinearRing* ring,
                                 bool close,
                                 int32_t& cx,
                                 int32_t& cy,
                                 std::vector<uint32_t>& out) const
{
  if (!ring)
    return;
  const int n = ring->getNumPoints();
  // Skip degenerate rings (need at least 2 points for a line, 4 for a polygon ring)
  if (n < 2)
    return;

  // MoveTo first vertex
  auto [x0, y0] = project(ring->getX(0), ring->getY(0));
  out.push_back(makeCmd(CMD_MOVETO, 1));
  out.push_back(zigzag(x0 - cx));
  out.push_back(zigzag(y0 - cy));
  cx = x0;
  cy = y0;

  // LineTo remaining vertices (exclude the closing duplicate if ring is closed)
  const int lineTo = close ? (n - 1) : n;
  if (lineTo > 1)
  {
    out.push_back(makeCmd(CMD_LINETO, lineTo - 1));
    for (int i = 1; i < lineTo; ++i)
    {
      auto [xi, yi] = project(ring->getX(i), ring->getY(i));
      out.push_back(zigzag(xi - cx));
      out.push_back(zigzag(yi - cy));
      cx = xi;
      cy = yi;
    }
  }

  if (close)
    out.push_back(makeCmd(CMD_CLOSEPATH, 1));
}

// ----------------------------------------------------------------------

void MVTLayerBuilder::encodeLineString(const OGRLineString* ls,
                                       bool close,
                                       int32_t& cx,
                                       int32_t& cy,
                                       std::vector<uint32_t>& out) const
{
  if (!ls)
    return;
  const int n = ls->getNumPoints();
  if (n < 2)
    return;

  auto [x0, y0] = project(ls->getX(0), ls->getY(0));
  out.push_back(makeCmd(CMD_MOVETO, 1));
  out.push_back(zigzag(x0 - cx));
  out.push_back(zigzag(y0 - cy));
  cx = x0;
  cy = y0;

  const int lineTo = close ? (n - 1) : n;
  if (lineTo > 1)
  {
    out.push_back(makeCmd(CMD_LINETO, lineTo - 1));
    for (int i = 1; i < lineTo; ++i)
    {
      auto [xi, yi] = project(ls->getX(i), ls->getY(i));
      out.push_back(zigzag(xi - cx));
      out.push_back(zigzag(yi - cy));
      cx = xi;
      cy = yi;
    }
  }

  if (close)
    out.push_back(makeCmd(CMD_CLOSEPATH, 1));
}

// ----------------------------------------------------------------------

std::vector<uint32_t> MVTLayerBuilder::encodeGeometry(const OGRGeometry& geom) const
{
  std::vector<uint32_t> out;
  int32_t cx = 0, cy = 0;

  const auto type = wkbFlatten(geom.getGeometryType());

  switch (type)
  {
    case wkbPoint:
    {
      const auto& pt = static_cast<const OGRPoint&>(geom);
      auto [tx, ty] = project(pt.getX(), pt.getY());
      out.push_back(makeCmd(CMD_MOVETO, 1));
      out.push_back(zigzag(tx - cx));
      out.push_back(zigzag(ty - cy));
      break;
    }

    case wkbMultiPoint:
    {
      const auto& mp = static_cast<const OGRMultiPoint&>(geom);
      // All points in one MoveTo command sequence (separate MoveTo per point)
      for (int i = 0; i < mp.getNumGeometries(); ++i)
      {
        const auto* pt = static_cast<const OGRPoint*>(mp.getGeometryRef(i));
        if (!pt || pt->IsEmpty())
          continue;
        auto [tx, ty] = project(pt->getX(), pt->getY());
        out.push_back(makeCmd(CMD_MOVETO, 1));
        out.push_back(zigzag(tx - cx));
        out.push_back(zigzag(ty - cy));
        cx = tx;
        cy = ty;
      }
      break;
    }

    case wkbLineString:
    {
      const auto& ls = static_cast<const OGRLineString&>(geom);
      encodeLineString(&ls, false, cx, cy, out);
      break;
    }

    case wkbMultiLineString:
    {
      const auto& mls = static_cast<const OGRMultiLineString&>(geom);
      for (int i = 0; i < mls.getNumGeometries(); ++i)
      {
        const auto* ls = static_cast<const OGRLineString*>(mls.getGeometryRef(i));
        if (!ls || ls->IsEmpty())
          continue;
        encodeLineString(ls, false, cx, cy, out);
      }
      break;
    }

    case wkbPolygon:
    {
      const auto& poly = static_cast<const OGRPolygon&>(geom);
      encodeRing(poly.getExteriorRing(), true, cx, cy, out);
      for (int i = 0; i < poly.getNumInteriorRings(); ++i)
        encodeRing(poly.getInteriorRing(i), true, cx, cy, out);
      break;
    }

    case wkbMultiPolygon:
    {
      const auto& mp = static_cast<const OGRMultiPolygon&>(geom);
      for (int i = 0; i < mp.getNumGeometries(); ++i)
      {
        const auto* poly = static_cast<const OGRPolygon*>(mp.getGeometryRef(i));
        if (!poly || poly->IsEmpty())
          continue;
        encodeRing(poly->getExteriorRing(), true, cx, cy, out);
        for (int j = 0; j < poly->getNumInteriorRings(); ++j)
          encodeRing(poly->getInteriorRing(j), true, cx, cy, out);
      }
      break;
    }

    case wkbGeometryCollection:
    {
      // Encode each sub-geometry in sequence (caller should check type matches)
      const auto& gc = static_cast<const OGRGeometryCollection&>(geom);
      for (int i = 0; i < gc.getNumGeometries(); ++i)
      {
        const auto* sub = gc.getGeometryRef(i);
        if (sub && !sub->IsEmpty())
        {
          auto sub_cmds = encodeGeometry(*sub);
          out.insert(out.end(), sub_cmds.begin(), sub_cmds.end());
        }
      }
      break;
    }

    default:
      break;
  }

  return out;
}

// ----------------------------------------------------------------------

vector_tile::Tile_GeomType MVTLayerBuilder::ogrTypeToMVT(const OGRGeometry& geom) const
{
  switch (wkbFlatten(geom.getGeometryType()))
  {
    case wkbPoint:
    case wkbMultiPoint:
      return vector_tile::Tile_GeomType_POINT;
    case wkbLineString:
    case wkbMultiLineString:
      return vector_tile::Tile_GeomType_LINESTRING;
    case wkbPolygon:
    case wkbMultiPolygon:
      return vector_tile::Tile_GeomType_POLYGON;
    default:
      return vector_tile::Tile_GeomType_UNKNOWN;
  }
}

// ----------------------------------------------------------------------

void MVTLayerBuilder::addFeature(const OGRGeometry& geom,
                                 const std::vector<std::pair<std::string, MVTValue>>& attrs)
{
  if (geom.IsEmpty())
    return;

  // For geometry collections, add each sub-geometry as a separate feature
  if (wkbFlatten(geom.getGeometryType()) == wkbGeometryCollection)
  {
    const auto& gc = static_cast<const OGRGeometryCollection&>(geom);
    for (int i = 0; i < gc.getNumGeometries(); ++i)
    {
      const auto* sub = gc.getGeometryRef(i);
      if (sub && !sub->IsEmpty())
        addFeature(*sub, attrs);
    }
    return;
  }

  auto geomType = ogrTypeToMVT(geom);
  if (geomType == vector_tile::Tile_GeomType_UNKNOWN)
    return;

  auto geometry = encodeGeometry(geom);
  if (geometry.empty())
    return;

  auto* feature = itsLayer->add_features();
  feature->set_type(geomType);
  for (uint32_t cmd : geometry)
    feature->add_geometry(cmd);

  // Encode attributes as (key_index, value_index) pairs
  for (const auto& [key, val] : attrs)
  {
    const uint32_t ki = internKey(key);
    const uint32_t vi = appendValue(val);
    feature->add_tags(ki);
    feature->add_tags(vi);
  }
}

// ======================================================================
// MVTTileBuilder
// ======================================================================

MVTTileBuilder::MVTTileBuilder(double xmin,
                               double ymin,
                               double xmax,
                               double ymax,
                               unsigned extent)
    : itsXMin(xmin), itsYMin(ymin), itsXMax(xmax), itsYMax(ymax), itsExtent(extent)
{
}

// ----------------------------------------------------------------------

MVTLayerBuilder& MVTTileBuilder::layer(const std::string& name)
{
  auto it = itsLayerIndex.find(name);
  if (it != itsLayerIndex.end())
    return itsBuilders[it->second];

  // Create a new proto Layer in the Tile
  auto* protoLayer = itsTile.add_layers();
  protoLayer->set_name(name);

  // Store a builder for it
  const size_t idx = itsBuilders.size();
  itsBuilders.emplace_back(protoLayer, itsXMin, itsYMin, itsXMax, itsYMax, itsExtent);
  itsLayerIndex[name] = idx;
  return itsBuilders[idx];
}

// ----------------------------------------------------------------------

std::string MVTTileBuilder::serialize() const
{
  std::string out;
  if (!itsTile.SerializeToString(&out))
    throw Fmi::Exception(BCP, "Failed to serialize Mapbox Vector Tile");
  return out;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
