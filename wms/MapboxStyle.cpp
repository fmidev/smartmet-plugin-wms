#include "MapboxStyle.h"
#include "StyleSheet.h"
#include <cstdlib>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// A CSS declaration value for a class (e.g. "fill", "stroke", "stroke-width"),
// or "" when absent or explicitly "none".
std::string classProperty(const StyleSheet& sheet, const std::string& cls, const std::string& prop)
{
  if (cls.empty())
    return "";
  const auto& decl = sheet.declarations("." + cls);
  auto it = decl.find(prop);
  if (it == decl.end() || it->second == "none")
    return "";
  return it->second;
}

// The class label of one level object (attributes.class, or a bare "class").
std::string levelClass(const Json::Value& level)
{
  if (level.isMember("attributes") && level["attributes"].isMember("class"))
    return level["attributes"]["class"].asString();
  if (level.isMember("class"))
    return level["class"].asString();
  return "";
}

// Parse a CSS length like "0.2px" or "1.5" to a number; fallback on parse error.
double parseWidth(const std::string& value, double fallback)
{
  if (value.empty())
    return fallback;
  char* end = nullptr;
  const double w = std::strtod(value.c_str(), &end);
  if (end == value.c_str())  // no digits consumed
    return fallback;
  return w;
}

Json::Value getClassExpr()
{
  Json::Value expr(Json::arrayValue);
  expr.append("get");
  expr.append("class");
  return expr;
}

// ["match", ["get","class"], cls, colour, …, defaultColour] over the levels whose
// class resolves to a non-empty value for `prop`. Returns Json::nullValue when no
// class matched (caller can then fall back to a parameter-level constant).
Json::Value colourMatch(const StyleSheet& sheet,
                        const Json::Value& levels,
                        const std::string& prop,
                        const std::string& defaultColour)
{
  Json::Value match(Json::arrayValue);
  match.append("match");
  match.append(getClassExpr());
  bool any = false;
  if (levels.isArray())
  {
    for (const auto& level : levels)
    {
      const std::string cls = levelClass(level);
      if (cls.empty())
        continue;
      const std::string colour = classProperty(sheet, cls, prop);
      if (colour.empty())
        continue;
      match.append(cls);
      match.append(colour);
      any = true;
    }
  }
  if (!any)
    return Json::nullValue;
  match.append(defaultColour);
  return match;
}

// Resolve a paint colour for a `prop` (fill / stroke): a per-class ["match",…]
// when the levels carry classes styled for that property, otherwise the
// parameter-level class (".Parameter") as a constant (uniformly-styled isolines),
// otherwise Json::nullValue when there is nothing to style.
Json::Value resolveColour(const StyleSheet& sheet,
                          const Json::Value& levels,
                          const std::string& parameter,
                          const std::string& prop,
                          const std::string& matchDefault)
{
  Json::Value match = colourMatch(sheet, levels, prop, matchDefault);
  if (!match.isNull())
    return match;
  const std::string constant = classProperty(sheet, parameter, prop);
  if (!constant.empty())
    return Json::Value(constant);
  return Json::nullValue;
}

// Per-class ["match",…] of stroke-width; Json::nullValue when no class carries one.
Json::Value widthMatch(const StyleSheet& sheet, const Json::Value& levels, double defaultWidth)
{
  Json::Value match(Json::arrayValue);
  match.append("match");
  match.append(getClassExpr());
  bool any = false;
  if (levels.isArray())
  {
    for (const auto& level : levels)
    {
      const std::string cls = levelClass(level);
      if (cls.empty() || classProperty(sheet, cls, "stroke").empty())
        continue;
      const std::string w = classProperty(sheet, cls, "stroke-width");
      if (w.empty())
        continue;
      match.append(cls);
      match.append(parseWidth(w, defaultWidth));
      any = true;
    }
  }
  if (!any)
    return Json::nullValue;
  match.append(defaultWidth);
  return match;
}

// Line width: per-class match if present, else the parameter-level stroke-width,
// else a uniform default.
Json::Value resolveWidth(const StyleSheet& sheet,
                         const Json::Value& levels,
                         const std::string& parameter,
                         double defaultWidth)
{
  Json::Value match = widthMatch(sheet, levels, defaultWidth);
  if (!match.isNull())
    return match;
  return Json::Value(parseWidth(classProperty(sheet, parameter, "stroke-width"), defaultWidth));
}

Json::Value vectorSource(const std::string& tileUrlTemplate)
{
  Json::Value source(Json::objectValue);
  source["type"] = "vector";
  Json::Value tiles(Json::arrayValue);
  tiles.append(tileUrlTemplate);
  source["tiles"] = tiles;
  source["maxzoom"] = 14;
  return source;
}
}  // namespace

std::string mapboxStyle(const std::string& styleName,
                        const std::string& tileUrlTemplate,
                        const std::vector<MapboxStyleLayer>& inputLayers)
{
  const std::string srcId = styleName;

  Json::Value layers(Json::arrayValue);
  int isobandCount = 0;
  int isolineCount = 0;

  for (const auto& in : inputLayers)
  {
    StyleSheet sheet;
    sheet.add(in.css);

    Json::Value layer(Json::objectValue);
    layer["source"] = srcId;
    layer["source-layer"] = in.sourceLayer;

    if (in.geometry == MapboxStyleLayer::Geometry::Isoband)
    {
      // Values outside every band → transparent.
      const Json::Value fill =
          resolveColour(sheet, in.levels, in.parameter, "fill", "rgba(0,0,0,0)");
      if (fill.isNull())
        continue;  // nothing styleable — skip rather than emit an empty layer
      Json::Value paint(Json::objectValue);
      paint["fill-color"] = fill;
      paint["fill-opacity"] = 0.85;

      const int idx = isobandCount++;
      layer["id"] = styleName + ":isobands" + (idx ? Json::Value(idx).asString() : "");
      layer["type"] = "fill";
      layer["paint"] = paint;
    }
    else  // Isoline → line
    {
      // Lines with no CSS stroke → transparent (invisible), same skip semantics.
      const Json::Value line =
          resolveColour(sheet, in.levels, in.parameter, "stroke", "rgba(0,0,0,0)");
      if (line.isNull())
        continue;
      Json::Value paint(Json::objectValue);
      paint["line-color"] = line;
      paint["line-width"] = resolveWidth(sheet, in.levels, in.parameter, 1.0);
      paint["line-opacity"] = 1.0;

      Json::Value layout(Json::objectValue);
      layout["line-cap"] = "round";
      layout["line-join"] = "round";

      const int idx = isolineCount++;
      layer["id"] = styleName + ":isolines" + (idx ? Json::Value(idx).asString() : "");
      layer["type"] = "line";
      layer["paint"] = paint;
      layer["layout"] = layout;
    }

    layers.append(layer);
  }

  Json::Value style(Json::objectValue);
  style["version"] = 8;
  style["name"] = styleName;
  Json::Value sources(Json::objectValue);
  sources[srcId] = vectorSource(tileUrlTemplate);
  style["sources"] = sources;
  style["layers"] = layers;

  Json::StreamWriterBuilder wb;
  wb["indentation"] = "  ";
  return Json::writeString(wb, style);
}

std::string mapboxStyleForIsobands(const std::string& layerName,
                                   const std::string& tileUrlTemplate,
                                   const Json::Value& isobandLevels,
                                   const std::string& css,
                                   const std::string& sourceLayer)
{
  MapboxStyleLayer layer;
  layer.geometry = MapboxStyleLayer::Geometry::Isoband;
  layer.sourceLayer = sourceLayer;
  layer.levels = isobandLevels;
  layer.css = css;
  return mapboxStyle(layerName, tileUrlTemplate, {layer});
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
