// ======================================================================
/*!
 * \brief OpenStreetMap layer implementation
 */
// ======================================================================

#ifndef WITHOUT_OSM

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
#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>
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

// Add an SVG element to a group. Text halos are marked with paint-order="stroke", which older
// librsvg versions (2.42 in RHEL8) ignore, drawing the halo over the text. Such text is drawn
// twice instead: first the halo alone, then the text without a stroke on top of it.
// The attributes of the rendered paths a layer may override. The stroke attributes apply to
// lines and polygon outlines, the fill attributes to polygon fills.
const std::set<std::string> overridable_attributes{
    "stroke", "stroke-width", "stroke-opacity", "stroke-dasharray", "fill", "fill-opacity"};

// Replace or add the overridden attributes of a path element
std::vector<std::pair<std::string, std::string>> apply_overrides(
    const std::string& theTag,
    const std::vector<std::pair<std::string, std::string>>& theAttributes,
    const std::vector<std::pair<std::string, std::string>>& theOverrides)
{
  if (theOverrides.empty() || theTag != "path")
    return theAttributes;

  auto value = [&](const std::string& name) -> std::string
  {
    for (const auto& attr : theAttributes)
      if (attr.first == name)
        return attr.second;
    return "none";
  };

  const bool stroked = (value("stroke") != "none");
  const bool filled = (value("fill") != "none");

  auto ret = theAttributes;
  for (const auto& [name, val] : theOverrides)
  {
    const bool is_stroke = (name.compare(0, 6, "stroke") == 0);
    if ((is_stroke && !stroked) || (!is_stroke && !filled))
      continue;
    auto pos =
        std::find_if(ret.begin(), ret.end(), [&](const auto& attr) { return attr.first == name; });
    if (pos != ret.end())
      pos->second = val;
    else
      ret.emplace_back(name, val);
  }
  return ret;
}

void addElement(CTPP::CDT& theGroup,
                const std::string& theTag,
                const std::vector<std::pair<std::string, std::string>>& theAttributes,
                const std::string& theContent)
{
  auto make_tag = [&](auto&& theSelect)
  {
    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<" + theTag;
    for (const auto& [name, value] : theAttributes)
      theSelect(tag_cdt["attributes"], name, value);
    if (theContent.empty())
      tag_cdt["end"] = "/>";
    else
      tag_cdt["end"] = ">" + theContent + "</" + theTag + ">";
    return tag_cdt;
  };

  const auto halo =
      std::find(theAttributes.begin(),
                theAttributes.end(),
                std::pair<std::string, std::string>{"paint-order", "stroke"}) != theAttributes.end();

  if (!halo)
  {
    theGroup.PushBack(make_tag([](CTPP::CDT& attrs, const std::string& name,
                                  const std::string& value) { attrs[name] = value; }));
    return;
  }

  // The halo: the stroke, and the fill in the stroke colour
  theGroup.PushBack(make_tag(
      [&](CTPP::CDT& attrs, const std::string& name, const std::string& value)
      {
        if (name == "paint-order" || name == "fill" || name == "fill-opacity")
          return;
        attrs[name] = value;
        if (name == "stroke")
          attrs["fill"] = value;
        else if (name == "stroke-opacity")
          attrs["fill-opacity"] = value;
      }));

  // The text without a stroke
  theGroup.PushBack(make_tag(
      [](CTPP::CDT& attrs, const std::string& name, const std::string& value)
      {
        if (name != "paint-order" && name.compare(0, 6, "stroke") != 0)
          attrs[name] = value;
      }));
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
  // Vector tile dataset: the newest file of the dataset
  if (isDataset())
  {
    if (auto* osm = theState.getOSMEngine())
      return osm->getDataTimestamp(itsOptions.dataset);
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
 * \brief Read the SVG attribute overrides of the rendered paths
 */
// ----------------------------------------------------------------------

void OSMLayer::parseOverrides(const Json::Value& theJson)
{
  if (theJson.isNull())
    return;
  if (!theJson.isObject())
    throw Fmi::Exception(BCP, "OSMLayer 'override' must be an object");

  for (const auto& name : theJson.getMemberNames())
  {
    if (overridable_attributes.count(name) == 0)
      throw Fmi::Exception(BCP, "OSMLayer 'override' does not support this attribute")
          .addParameter("Attribute", name);

    const auto& json = theJson[name];
    std::string value;
    if (json.isString())
      value = json.asString();
    else if (json.isNumeric())
      value = Fmi::to_string(json.asDouble());
    else
      throw Fmi::Exception(BCP, "OSMLayer 'override' values must be strings or numbers")
          .addParameter("Attribute", name);

    // The values end up in SVG attributes
    itsOverrides.emplace_back(name, Fmi::safexmlescape(value));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 *
 * Vector tile dataset rendered with a MapLibre style:
 * {
 *   "layer_type": "osm",
 *   "dataset": "protomaps",          // dataset in the OSM engine data directory
 *   "style": "protomaps-white",      // style in the OSM engine style directory
 *   "labels": true,                  // render the symbol layers of the style
 *   "icons": true,                   // dots for point symbols with icons
 *   "include": ["earth", "water*"],  // style layer ids or prefixes to render
 *   "exclude": ["pois", "roads_*"],  // style layer ids or prefixes to skip
 *   "zoom": 6,                       // fixed style zoom level instead of automatic
 *   "zoom_offset": 0,                // adjustment to the automatic zoom level
 *   "font_family": "Noto Sans, sans-serif",  // fallback fonts
 *   "attribution": false,            // draw "© OpenStreetMap" into the image
 *   "override": { "stroke": "#303030", "stroke-width": 1.5 }  // line and fill attributes
 * }
 *
 * PostGIS data loaded with osm2pgsql:
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

    JsonTools::remove_string(itsOptions.dataset, theJson, "dataset");
    if (isDataset())
    {
      JsonTools::remove_string(itsOptions.style, theJson, "style");
      if (itsOptions.style.empty())
        throw Fmi::Exception(BCP, "OSMLayer with a dataset requires a 'style'");
      JsonTools::remove_bool(itsOptions.labels, theJson, "labels");
      JsonTools::remove_bool(itsOptions.icons, theJson, "icons");
      JsonTools::remove_double(itsOptions.zoomOffset, theJson, "zoom_offset");
      std::optional<double> zoom;
      JsonTools::remove_double(zoom, theJson, "zoom");
      itsOptions.zoom = zoom;
      JsonTools::remove_string(itsOptions.fontFamily, theJson, "font_family");
      JsonTools::remove_bool(itsAttribution, theJson, "attribution");
      parseOverrides(JsonTools::remove(theJson, "override"));

      for (const auto* name : {"include", "exclude"})
      {
        auto json = JsonTools::remove(theJson, name);
        if (json.isNull())
          continue;
        if (!json.isArray())
          throw Fmi::Exception(BCP, fmt::format("OSMLayer '{}' must be an array", name));
        auto& list = (std::string(name) == "include" ? itsOptions.include : itsOptions.exclude);
        for (const auto& id : json)
          list.push_back(id.asString());
      }

      // The precision of the pixel coordinates
      itsOptions.precision = static_cast<int>(precision);
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

    if (isDataset())
    {
      generateDataset(theGlobals, theLayersCdt, theState);
      return;
    }

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
          text_cdt["cdata"] = Fmi::safexmlescape(name);
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

    // Vector tile datasets return their tiles directly via getRawMVTBytes()
    if (isDataset())
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
 * \brief Return the MVT tile of a vector tile dataset
 *
 * Used when the product consists of a dataset layer. The tile coordinates
 * are stored in State by the request handler for WebMercatorQuad requests.
 */
// ----------------------------------------------------------------------

std::optional<std::string> OSMLayer::getRawMVTBytes(const State& theState) const
{
  if (!isDataset() || !theState.hasTileCoords())
    return std::nullopt;
  auto* osm = theState.getOSMEngine();
  if (!osm)
    throw Fmi::Exception(BCP, "OSM engine is not available");
  auto tile = osm->getVectorTile(
      itsOptions.dataset, theState.getTileZ(), theState.getTileX(), theState.getTileY());
  if (!tile)
    return std::string();  // empty tile outside the dataset
  return tile;
}

// ----------------------------------------------------------------------
/*!
 * \brief Render a vector tile dataset with the OSM engine
 */
// ----------------------------------------------------------------------

void OSMLayer::generateDataset(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // Only SVG based output formats are supported
    const auto& type = theState.getType();
    if (type == "geojson" || type == "topojson" || type == "kml")
      return;

    auto* osm = theState.getOSMEngine();
    if (!osm)
      throw Fmi::Exception(BCP, "OSM engine is not available");

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "OSMLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    auto options = itsOptions;
    options.margin = std::max(xmargin, ymargin);

    auto scene = osm->renderMap(options, projection.getCRS(), projection.getBox());

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    theState.addAttributes(theGlobals, group_cdt, attributes);

    for (const auto& element : scene->elements)
      addElement(group_cdt["tags"],
                 element.tag,
                 apply_overrides(element.tag, element.attributes, itsOverrides),
                 element.content);

    if (itsAttribution && !scene->attribution.empty())
      addElement(group_cdt["tags"],
                 "text",
                 {{"x", Fmi::to_string(static_cast<long>(scene->width) - 4)},
                  {"y", Fmi::to_string(static_cast<long>(scene->height) - 4)},
                  {"text-anchor", "end"},
                  {"font-family", Fmi::safexmlescape(itsOptions.fontFamily)},
                  {"font-size", "10"},
                  {"fill", "#333333"},
                  {"stroke", "#ffffff"},
                  {"stroke-width", "2"},
                  {"paint-order", "stroke"}},
                 Fmi::safexmlescape(scene->attribution));

    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OSMLayer failed to render dataset")
        .addParameter("dataset", itsOptions.dataset)
        .addParameter("style", itsOptions.style);
  }
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
    Fmi::hash_combine(hash, Fmi::hash_value(getDataTimestamp(theState)));
    if (isDataset())
    {
      const auto& o = itsOptions;
      Fmi::hash_combine(hash, Fmi::hash_value(o.dataset));
      Fmi::hash_combine(hash, Fmi::hash_value(o.style));
      Fmi::hash_combine(hash, Fmi::hash_value(o.labels));
      Fmi::hash_combine(hash, Fmi::hash_value(o.icons));
      Fmi::hash_combine(hash, Fmi::hash_value(o.zoomOffset));
      if (o.zoom)
        Fmi::hash_combine(hash, Fmi::hash_value(*o.zoom));
      Fmi::hash_combine(hash, Fmi::hash_value(o.fontFamily));
      for (const auto& id : o.include)
        Fmi::hash_combine(hash, Fmi::hash_value(id));
      for (const auto& id : o.exclude)
        Fmi::hash_combine(hash, Fmi::hash_value(id) + 1);
      Fmi::hash_combine(hash, Fmi::hash_value(itsAttribution));
      for (const auto& [name, value] : itsOverrides)
      {
        Fmi::hash_combine(hash, Fmi::hash_value(name));
        Fmi::hash_combine(hash, Fmi::hash_value(value));
      }

      // The dataset version changes when its files change, the style when it is edited
      if (auto* osm = theState.getOSMEngine())
      {
        auto info = osm->getDatasetInfo(o.dataset);
        if (info)
          Fmi::hash_combine(hash, Fmi::hash_value(info->version));
        Fmi::hash_combine(hash, Fmi::hash_value(osm->getStyle(o.style)->modified()));
      }
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

#endif  // WITHOUT_OSM
