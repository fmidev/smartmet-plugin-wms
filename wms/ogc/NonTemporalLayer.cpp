#include "NonTemporalLayer.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{
// Map options from the map settings of a product
Engine::Gis::MapOptions map_options(const Json::Value& map_json)
{
  Json::Value nulljson;
  Engine::Gis::MapOptions mopt;
  auto dbparam = map_json.get("pgname", nulljson);
  if (!dbparam.isNull())
    mopt.pgname = dbparam.asString();
  dbparam = map_json.get("schema", nulljson);
  if (!dbparam.isNull())
    mopt.schema = dbparam.asString();
  dbparam = map_json.get("table", nulljson);
  if (!dbparam.isNull())
    mopt.table = dbparam.asString();
  dbparam = map_json.get("where", nulljson);
  if (!dbparam.isNull())
    mopt.where = dbparam.asString();
  return mopt;
}

// Extend the bounding box to cover the envelope of the geometry
void extend_bbox(const OGRGeometry& geom,
                 std::optional<double>& xMin,
                 std::optional<double>& xMax,
                 std::optional<double>& yMin,
                 std::optional<double>& yMax)
{
  OGREnvelope envelope;
  geom.getEnvelope(&envelope);
  if (!xMin)
  {
    xMin = envelope.MinX;
    xMax = envelope.MaxX;
    yMin = envelope.MinY;
    yMax = envelope.MaxY;
  }
  else
  {
    xMin = std::min(*xMin, envelope.MinX);
    xMax = std::max(*xMax, envelope.MaxX);
    yMin = std::min(*yMin, envelope.MinY);
    yMax = std::max(*yMax, envelope.MaxY);
  }
}
}  // namespace

MapLayer::MapLayer(const LayerConfig& config, const Json::Value& json)
    : NonTemporalLayer(config)
{
  Json::Value nulljson;

  std::optional<double> xMin;
  std::optional<double> xMax;
  std::optional<double> yMin;
  std::optional<double> yMax;
  if (json.isArray())
  {
    Fmi::SpatialReference sr(4326);
    for (const auto& j : json)
    {
      const auto& map_json = j.get("map", nulljson);

      if (!map_json.isNull())
      {
        Engine::Gis::MapOptions mopt = map_options(map_json);
        auto geom = config.gisEngine()->getShape(&sr, mopt);
        if (geom)
        {
          extend_bbox(*geom, xMin, xMax, yMin, yMax);
        }
      }
    }
  }

  // BBOX is read from geometry dimension (database),
  geographicBoundingBox.xMin = (xMin ? *xMin : -90);
  geographicBoundingBox.yMin = (yMin ? *yMin : -90);
  geographicBoundingBox.xMax = (xMax ? *xMax : 180);
  geographicBoundingBox.yMax = (yMax ? *yMax : 90);
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
