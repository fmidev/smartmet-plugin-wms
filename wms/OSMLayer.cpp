// ======================================================================
/*!
 * \brief OpenStreetMap layer implementation
 */
// ======================================================================

#include "OSMLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "MapboxVectorTile.h"
#include "State.h"
#include "XYTransformation.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <ogr_geometry.h>
#include <chrono>
#include <cmath>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{

// Return the string value of a feature attribute, or empty string if missing/non-string
std::string getAttribute(const Fmi::Feature& feature, const std::string& key)
{
  auto it = feature.attributes.find(key);
  if (it == feature.attributes.end())
    return {};
  return std::visit(
      [](const auto& v) -> std::string
      {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>)
          return v;
        else if constexpr (std::is_same_v<T, int>)
          return Fmi::to_string(v);
        else if constexpr (std::is_same_v<T, double>)
          return Fmi::to_string(v);
        else  // Fmi::DateTime
          return Fmi::to_iso_string(v);
      },
      it->second);
}

// Convert Fmi::Attribute to MVTValue for Mapbox Vector Tile output
MVTValue toMVTValue(const Fmi::Attribute& attr)
{
  return std::visit(
      [](const auto& v) -> MVTValue
      {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, int>)
          return static_cast<int64_t>(v);
        else if constexpr (std::is_same_v<T, double>)
          return v;
        else if constexpr (std::is_same_v<T, std::string>)
          return v;
        else  // Fmi::DateTime
          return Fmi::to_iso_string(v);
      },
      attr);
}

// Simple label collision box in SVG pixel coordinates
struct LabelBox
{
  double x1, y1, x2, y2;

  bool overlaps(const LabelBox& o) const
  {
    return !(x2 < o.x1 || o.x2 < x1 || y2 < o.y1 || o.y2 < y1);
  }
};

// Estimate label box at (cx, cy) for given text; uses heuristic character dimensions
LabelBox makeLabelBox(double cx, double cy, const std::string& text)
{
  const double char_width = 7.0;
  const double char_height = 14.0;
  const double w = text.size() * char_width;
  return {cx - w / 2.0, cy - char_height, cx + w / 2.0, cy};
}

// Return SVG-space centroid of a polygon or multipolygon, or std::nullopt on failure
std::optional<std::pair<double, double>> polygonCentroid(const OGRGeometry& geom,
                                                          const Projection& projection)
{
  OGRPoint centroid;
  if (geom.Centroid(&centroid) != OGRERR_NONE)
    return {};
  double x = centroid.getX();
  double y = centroid.getY();
  XYTransformation xform(projection);
  xform.transform(x, y);
  return {{x, y}};
}

// Return SVG-space midpoint of a linestring (midpoint of its total arc length)
std::optional<std::pair<double, double>> lineMidpoint(const OGRGeometry& geom,
                                                       const Projection& projection)
{
  const OGRLineString* ls = nullptr;
  if (wkbFlatten(geom.getGeometryType()) == wkbLineString)
  {
    ls = geom.toLineString();
  }
  else if (wkbFlatten(geom.getGeometryType()) == wkbMultiLineString)
  {
    // Pick the longest segment
    const auto* ml = geom.toMultiLineString();
    double maxLen = -1;
    for (auto* sub : *ml)
    {
      double len = sub->get_Length();
      if (len > maxLen)
      {
        maxLen = len;
        ls = sub;
      }
    }
  }
  if (!ls || ls->getNumPoints() < 2)
    return {};

  double totalLen = ls->get_Length();
  double half = totalLen / 2.0;
  double acc = 0;
  for (int i = 1; i < ls->getNumPoints(); ++i)
  {
    OGRPoint p0, p1;
    ls->getPoint(i - 1, &p0);
    ls->getPoint(i, &p1);
    double seg =
        std::hypot(p1.getX() - p0.getX(), p1.getY() - p0.getY());
    if (acc + seg >= half)
    {
      double t = (half - acc) / seg;
      double x = p0.getX() + t * (p1.getX() - p0.getX());
      double y = p0.getY() + t * (p1.getY() - p0.getY());
      XYTransformation xform(projection);
      xform.transform(x, y);
      return {{x, y}};
    }
    acc += seg;
  }
  return {};
}

// Return SVG-space position of a point geometry
std::optional<std::pair<double, double>> pointPosition(const OGRGeometry& geom,
                                                        const Projection& projection)
{
  if (wkbFlatten(geom.getGeometryType()) != wkbPoint)
    return {};
  const auto* pt = geom.toPoint();
  double x = pt->getX();
  double y = pt->getY();
  XYTransformation xform(projection);
  xform.transform(x, y);
  return {{x, y}};
}

}  // anonymous namespace

// ----------------------------------------------------------------------
/*!
 * \brief FeatureSet hash value (config-only, not data-dependent)
 */
// ----------------------------------------------------------------------

std::size_t OSMLayer::FeatureSet::hash_value() const
{
  auto hash = Fmi::hash_value(mapOptions.pgname);
  Fmi::hash_combine(hash, Fmi::hash_value(mapOptions.schema));
  Fmi::hash_combine(hash, Fmi::hash_value(mapOptions.table));
  Fmi::hash_combine(hash, Fmi::hash_value(mapOptions.where));
  Fmi::hash_combine(hash, Fmi::hash_value(mapOptions.minarea));
  Fmi::hash_combine(hash, Fmi::hash_value(mapOptions.mindistance));
  Fmi::hash_combine(hash, Fmi::hash_value(lines));
  Fmi::hash_combine(hash, Fmi::hash_value(labels));
  Fmi::hash_combine(hash, Fmi::hash_value(mvt_layer_name));
  Fmi::hash_combine(hash, Fmi::hash_value(css));
  return hash;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the OSM data age as seconds-since-epoch from timestamp file
 *
 * Returns 0 if the file does not exist (no caching effect beyond
 * configuration hash).
 */
// ----------------------------------------------------------------------

std::int64_t OSMLayer::getDataTimestamp(const State& theState) const
{
  // PMTiles backend: get timestamp from the OSM engine (file mtime)
  if (itsBackend == "pmtiles")
  {
    if (auto* osm = theState.getOSMEngine())
      return osm->getDataTimestamp(itsOSMSource);
    return 0;
  }

  // PostGIS backend: use the filesystem timestamp file
  if (itsTimestampFile.empty())
    return 0;
  std::error_code ec;
  auto ftime = std::filesystem::last_write_time(itsTimestampFile, ec);
  if (ec)
    return 0;
  return std::chrono::duration_cast<std::chrono::seconds>(
             ftime.time_since_epoch())
      .count();
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 *
 * Expected JSON structure:
 * {
 *   "layer_type": "osm",
 *   "timestamp_file": "/var/smartmet/osm/scandinavia.timestamp",
 *   "precision": 1.0,
 *   "feature_sets": [
 *     {
 *       "pgname":   "postgis",
 *       "schema":   "osm_scandinavia",
 *       "table":    "planet_osm_polygon",
 *       "where":    "natural = 'water'",
 *       "fieldnames": ["name"],
 *       "lines":    false,
 *       "labels":   false,
 *       "minarea":  1000,
 *       "mvt_layer_name": "water",
 *       "css": "osm_water"
 *     },
 *     ...
 *   ]
 * }
 */
// ----------------------------------------------------------------------

void OSMLayer::init(Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "OSMLayer JSON is not an object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("map");
    JsonTools::remove_double(precision, theJson, "precision");

    JsonTools::remove_string(itsBackend, theJson, "backend");
    if (itsBackend != "postgis" && itsBackend != "pmtiles")
      throw Fmi::Exception(BCP, "OSMLayer: 'backend' must be 'postgis' or 'pmtiles'");

    if (itsBackend == "pmtiles")
    {
      // PMTiles mode: just need the source name; feature_sets are not used
      JsonTools::remove_string(itsOSMSource, theJson, "source");
      if (itsOSMSource.empty())
        throw Fmi::Exception(BCP, "OSMLayer pmtiles backend requires 'source'");
      return;
    }

    // PostGIS backend
    std::string tsfile;
    JsonTools::remove_string(tsfile, theJson, "timestamp_file");
    if (!tsfile.empty())
      itsTimestampFile = tsfile;

    auto json = JsonTools::remove(theJson, "feature_sets");
    if (json.isNull() || !json.isArray())
      throw Fmi::Exception(BCP, "OSMLayer postgis backend requires a 'feature_sets' array");

    for (Json::ArrayIndex i = 0; i < json.size(); ++i)
    {
      Json::Value& fsJson = json[i];
      if (!fsJson.isObject())
        throw Fmi::Exception(BCP, "Each OSMLayer feature_set must be a JSON object");

      FeatureSet fs;

      JsonTools::remove_string(fs.mapOptions.pgname, fsJson, "pgname");
      JsonTools::remove_string(fs.mapOptions.schema, fsJson, "schema");
      JsonTools::remove_string(fs.mapOptions.table, fsJson, "table");
      JsonTools::remove_string(fs.mapOptions.where, fsJson, "where");
      JsonTools::remove_double(fs.mapOptions.minarea, fsJson, "minarea");
      JsonTools::remove_double(fs.mapOptions.mindistance, fsJson, "mindistance");

      if (fs.mapOptions.pgname.empty())
        throw Fmi::Exception(BCP, "OSMLayer feature_set requires 'pgname'");
      if (fs.mapOptions.schema.empty())
        throw Fmi::Exception(BCP, "OSMLayer feature_set requires 'schema'");
      if (fs.mapOptions.table.empty())
        throw Fmi::Exception(BCP, "OSMLayer feature_set requires 'table'");

      JsonTools::remove_bool(fs.lines, fsJson, "lines");
      JsonTools::remove_bool(fs.labels, fsJson, "labels");

      // Collect fieldnames (always include "name" when labels are enabled)
      auto fieldJson = JsonTools::remove(fsJson, "fieldnames");
      if (!fieldJson.isNull() && fieldJson.isArray())
        for (Json::ArrayIndex j = 0; j < fieldJson.size(); ++j)
          fs.mapOptions.fieldnames.insert(fieldJson[j].asString());
      if (fs.labels)
        fs.mapOptions.fieldnames.insert("name");

      JsonTools::remove_string(fs.css, fsJson, "css");
      JsonTools::remove_string(fs.mvt_layer_name, fsJson, "mvt_layer_name");
      if (fs.mvt_layer_name.empty())
        fs.mvt_layer_name = fs.mapOptions.table;

      // Parse per-feature-set SVG attributes
      auto attrJson = JsonTools::remove(fsJson, "attributes");
      if (!attrJson.isNull())
        fs.attributes.init(attrJson, theConfig);

      itsFeatureSets.push_back(std::move(fs));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OSMLayer::init failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate SVG output
 */
// ----------------------------------------------------------------------

void OSMLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "OSMLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    const auto& crs = projection.getCRS();
    const Fmi::Box& box = projection.getBox();
    const auto clipbox = getClipBox(box);
    const auto& gis = theState.getGisEngine();

    // Outer <g> groups all feature sets under the layer's attributes
    CTPP::CDT outer_cdt(CTPP::CDT::HASH_VAL);
    outer_cdt["start"] = "<g";
    outer_cdt["end"] = "</g>";
    theState.addAttributes(theGlobals, outer_cdt, attributes);

    int mapid = 1;

    for (const auto& fs : itsFeatureSets)
    {
      if (fs.css)
      {
        std::string name = theState.getCustomer() + "/" + *fs.css;
        theGlobals["css"][name] = theState.getStyle(*fs.css);
      }

      // Inner <g> groups features within this feature set
      CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
      group_cdt["start"] = "<g";
      group_cdt["end"] = "</g>";
      theState.addAttributes(theGlobals, group_cdt, fs.attributes);

      if (fs.mapOptions.fieldnames.empty() && !fs.labels)
      {
        // Fast path: no per-feature attributes needed — fetch unified geometry
        OGRGeometryPtr geom = gis.getShape(&crs, fs.mapOptions);
        if (!geom || geom->IsEmpty())
        {
          outer_cdt["tags"].PushBack(group_cdt);
          continue;
        }
        geom.reset(geom->clone());
        Fmi::OGR::normalizeWindingOrder(geom.get());
        if (fs.lines)
          geom.reset(Fmi::OGR::lineclip(*geom, clipbox));
        else
          geom.reset(Fmi::OGR::polyclip(*geom, clipbox));

        if (!geom || geom->IsEmpty())
        {
          outer_cdt["tags"].PushBack(group_cdt);
          continue;
        }

        std::string iri = qid + Fmi::to_string(mapid++);
        CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
        map_cdt["iri"] = iri;
        map_cdt["type"] = Geometry::name(*geom, theState.getType());
        map_cdt["layertype"] = "osm";
        map_cdt["data"] = Geometry::toString(*geom, theState.getType(), box, crs, precision);
        theState.addPresentationAttributes(map_cdt, fs.css, fs.attributes);
        theGlobals["paths"][iri] = map_cdt;

        CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
        tag_cdt["start"] = "<use";
        tag_cdt["end"] = "/>";
        tag_cdt["attributes"]["xlink:href"] = "#" + iri;
        group_cdt["tags"].PushBack(tag_cdt);
      }
      else
      {
        // Per-feature path: fetch individual features with attributes
        Fmi::Features features = gis.getFeatures(crs, fs.mapOptions);

        // Track placed labels for simple collision avoidance
        std::vector<LabelBox> placed_labels;

        for (const auto& feat : features)
        {
          if (!feat->geom || feat->geom->IsEmpty())
            continue;

          OGRGeometryPtr geom(feat->geom->clone());
          Fmi::OGR::normalizeWindingOrder(geom.get());

          if (fs.lines)
            geom.reset(Fmi::OGR::lineclip(*geom, clipbox));
          else
            geom.reset(Fmi::OGR::polyclip(*geom, clipbox));

          if (!geom || geom->IsEmpty())
            continue;

          std::string iri = qid + Fmi::to_string(mapid++);
          CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
          map_cdt["iri"] = iri;
          map_cdt["type"] = Geometry::name(*geom, theState.getType());
          map_cdt["layertype"] = "osm";
          map_cdt["data"] = Geometry::toString(*geom, theState.getType(), box, crs, precision);
          theState.addPresentationAttributes(map_cdt, fs.css, fs.attributes);
          theGlobals["paths"][iri] = map_cdt;

          CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
          tag_cdt["start"] = "<use";
          tag_cdt["end"] = "/>";
          tag_cdt["attributes"]["xlink:href"] = "#" + iri;
          group_cdt["tags"].PushBack(tag_cdt);

          // Optionally place a label
          if (!fs.labels)
            continue;

          std::string name = getAttribute(*feat, "name");
          if (name.empty())
            continue;

          // Compute SVG position depending on geometry type
          std::optional<std::pair<double, double>> pos;
          const auto gt = wkbFlatten(feat->geom->getGeometryType());
          if (gt == wkbPoint || gt == wkbMultiPoint)
            pos = pointPosition(*feat->geom, projection);
          else if (gt == wkbLineString || gt == wkbMultiLineString)
            pos = lineMidpoint(*feat->geom, projection);
          else
            pos = polygonCentroid(*feat->geom, projection);

          if (!pos)
            continue;

          auto [cx, cy] = *pos;
          LabelBox lb = makeLabelBox(cx, cy, name);

          // Skip label if it overlaps an already-placed one
          bool collision = false;
          for (const auto& placed : placed_labels)
          {
            if (lb.overlaps(placed))
            {
              collision = true;
              break;
            }
          }
          if (collision)
            continue;

          placed_labels.push_back(lb);

          CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
          text_cdt["start"] = "<text";
          text_cdt["end"] = "</text>";
          text_cdt["cdata"] = name;
          text_cdt["attributes"]["x"] = Fmi::to_string(static_cast<long>(std::round(cx)));
          text_cdt["attributes"]["y"] = Fmi::to_string(static_cast<long>(std::round(cy)));
          theState.addAttributes(theGlobals, text_cdt, fs.attributes);
          group_cdt["tags"].PushBack(text_cdt);
        }
      }

      outer_cdt["tags"].PushBack(group_cdt);
    }

    theLayersCdt.PushBack(outer_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OSMLayer::generate failed!").addParameter("qid", qid);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add features to a Mapbox Vector Tile builder
 */
// ----------------------------------------------------------------------

void OSMLayer::addMVTLayer(MVTTileBuilder& theBuilder, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // PMTiles backend: tile bytes come from the OSM engine; the MVTTileBuilder
    // cannot accept raw pre-encoded bytes directly (it encodes from OGR geometry).
    // For MVT output the tiles/Handler.cpp serializes the builder; for PMTiles
    // the OSMLayer bypasses the builder entirely — the raw bytes are returned by
    // the handler calling getRawMVTTile() instead. See tiles/Handler.cpp.
    // Nothing to do here for the PMTiles path.
    if (itsBackend == "pmtiles")
      return;

    const auto& crs = projection.getCRS();
    const Fmi::Box& box = projection.getBox();
    const auto clipbox = getClipBox(box);
    const auto& gis = theState.getGisEngine();

    for (const auto& fs : itsFeatureSets)
    {
      auto& mvtLayer = theBuilder.layer(fs.mvt_layer_name);

      if (fs.mapOptions.fieldnames.empty() && !fs.labels)
      {
        // No attributes needed: fetch as unified geometry, no feature properties
        OGRGeometryPtr geom = gis.getShape(&crs, fs.mapOptions);
        if (!geom || geom->IsEmpty())
          continue;
        geom.reset(geom->clone());
        Fmi::OGR::normalizeWindingOrder(geom.get());
        if (fs.lines)
          geom.reset(Fmi::OGR::lineclip(*geom, clipbox));
        else
          geom.reset(Fmi::OGR::polyclip(*geom, clipbox));
        if (!geom || geom->IsEmpty())
          continue;
        mvtLayer.addFeature(*geom, {});
      }
      else
      {
        // Per-feature: include requested OSM tag attributes
        Fmi::Features features = gis.getFeatures(crs, fs.mapOptions);

        for (const auto& feat : features)
        {
          if (!feat->geom || feat->geom->IsEmpty())
            continue;

          OGRGeometryPtr geom(feat->geom->clone());
          Fmi::OGR::normalizeWindingOrder(geom.get());
          if (fs.lines)
            geom.reset(Fmi::OGR::lineclip(*geom, clipbox));
          else
            geom.reset(Fmi::OGR::polyclip(*geom, clipbox));
          if (!geom || geom->IsEmpty())
            continue;

          // Build MVT attribute list from feature attributes
          std::vector<std::pair<std::string, MVTValue>> attrs;
          attrs.reserve(feat->attributes.size());
          for (const auto& [key, val] : feat->attributes)
            attrs.emplace_back(key, toMVTValue(val));

          mvtLayer.addFeature(*geom, attrs);
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OSMLayer::addMVTLayer failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return raw MVT tile bytes for the PMTiles backend (zero-copy)
 */
// ----------------------------------------------------------------------

std::optional<Engine::OSM::TileData> OSMLayer::getRawMVTTile(
    uint8_t z, uint32_t x, uint32_t y, const State& theState) const
{
  if (itsBackend != "pmtiles")
    return {};
  auto* osm = theState.getOSMEngine();
  if (!osm)
    return {};
  return osm->getTile(itsOSMSource, z, x, y);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return raw MVT bytes for Product::generateMVT() passthrough
 *
 * Used when the entire product is a PMTiles-backed OSMLayer. The tile
 * coordinates must have been stored in State by the request handler.
 */
// ----------------------------------------------------------------------

std::optional<std::string> OSMLayer::getRawMVTBytes(const State& theState) const
{
  if (itsBackend != "pmtiles" || !theState.hasTileCoords())
    return std::nullopt;
  auto tileData = getRawMVTTile(
      theState.getTileZ(), theState.getTileX(), theState.getTileY(), theState);
  if (!tileData)
    return std::nullopt;
  return std::string(reinterpret_cast<const char*>(tileData->data), tileData->size);
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo — not implemented
 */
// ----------------------------------------------------------------------

void OSMLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value — drives HTTP ETag
 *
 * Combines layer configuration hash with the OSM data timestamp so that
 * ETags change automatically after a new osm2pgsql import.
 */
// ----------------------------------------------------------------------

std::size_t OSMLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(itsBackend));
    Fmi::hash_combine(hash, Fmi::hash_value(getDataTimestamp(theState)));
    if (itsBackend == "pmtiles")
    {
      Fmi::hash_combine(hash, Fmi::hash_value(itsOSMSource));
    }
    else
    {
      Fmi::hash_combine(hash, Fmi::hash_value(precision));
      Fmi::hash_combine(hash, Fmi::hash_value(itsTimestampFile.string()));
      for (const auto& fs : itsFeatureSets)
        Fmi::hash_combine(hash, fs.hash_value());
    }
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OSMLayer::hash_value failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
