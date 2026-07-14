// ======================================================================
/*!
 * \brief Generate a MapLibre/Mapbox GL style document for a Dali isoband layer.
 *
 * Single source of truth: the wms-conf isoband level table (lolimit/hilimit/
 * class) + its CSS colour ramp. The class is exactly the attribute the OGC API
 * Tiles MVT output already carries on each polygon ("class"), so the generated
 * `fill-color` is a `["match", ["get","class"], …]` expression resolved from the
 * CSS via the existing StyleSheet parser. No colours are duplicated anywhere.
 *
 * This is the building block for serving styles through OGC API - Styles with the
 * Mapbox style encoding (application/vnd.mapbox.style+json), consumed natively by
 * MapLibre. See OGC-API-STYLES.md for the endpoint wiring.
 */
// ======================================================================

#pragma once

#include <json/json.h>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
/**
 * One MVT (vector) layer to style, resolved from a Dali layer definition.
 *
 * The MVT output tags every feature with a "class" attribute; the layer's CSS
 * maps that class to fill / stroke / stroke-width — exactly what the raster
 * renderer uses. So a style layer is just an assembly of those class→property
 * lookups into Mapbox "match" expressions, keeping the CSS the single source of
 * truth.
 */
struct MapboxStyleLayer
{
  enum class Geometry
  {
    Isoband,  // → Mapbox "fill" layer (fill-color from CSS fill)
    Isoline   // → Mapbox "line" layer (line-color/-width from CSS stroke)
  };

  Geometry geometry = Geometry::Isoband;
  std::string sourceLayer;  // MVT source-layer name (the Dali layer qid)
  std::string parameter;    // Dali parameter — the fallback CSS class ("."+parameter)
                            // used when the levels carry no per-value class (common
                            // for isolines styled uniformly)
  Json::Value levels;       // isobands/isolines array; each has attributes.class
  std::string css;          // the layer's CSS (class → fill/stroke/stroke-width)
};

/**
 * Build a Mapbox GL style JSON that renders a product's MVT tiles in the real
 * wms-conf colours: one shared vector source (the MVT template) and one style
 * layer per input, in order.
 *
 * @param styleName        Style / collection name (used for ids and source id).
 * @param tileUrlTemplate  MVT tile URL template, e.g.
 *                         ".../collections/<id>/tiles/EPSG:3857/{tileMatrix}/{tileRow}/{tileCol}?f=application/vnd.mapbox-vector-tile"
 * @param layers           The MVT layers to style (isoband/isoline …).
 * @return                 Mapbox GL style document (JSON string).
 */
std::string mapboxStyle(const std::string& styleName,
                        const std::string& tileUrlTemplate,
                        const std::vector<MapboxStyleLayer>& layers);

/**
 * Back-compat convenience for a single isoband layer.
 */
std::string mapboxStyleForIsobands(const std::string& layerName,
                                   const std::string& tileUrlTemplate,
                                   const Json::Value& isobandLevels,
                                   const std::string& css,
                                   const std::string& sourceLayer = "l");

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
