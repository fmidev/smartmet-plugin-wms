#include "WMSNonTemporalLayer.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
WMSMapLayer::WMSMapLayer(const WMSConfig& config, const Json::Value& json)
    : WMSNonTemporalLayer(config)
{
  Json::Value nulljson;

  boost::optional<double> xMin;
  boost::optional<double> xMax;
  boost::optional<double> yMin;
  boost::optional<double> yMax;
  if (json.isArray())
  {
    Fmi::SpatialReference sr(4326);
    for (const auto& j : json)
    {
      const auto& map_json = j.get("map", nulljson);

      if (!map_json.isNull())
      {
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
        auto geom = config.gisEngine()->getShape(&sr, mopt);
        if (geom)
        {
          OGREnvelope envelope;
          geom->getEnvelope(&envelope);
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
      }
    }
  }

  // BBOX is read from geometry dimension (database),
  geographicBoundingBox.xMin = (xMin ? *xMin : -90);
  geographicBoundingBox.yMin = (yMin ? *yMin : -90);
  geographicBoundingBox.xMax = (xMax ? *xMax : 180);
  geographicBoundingBox.yMax = (yMax ? *yMax : 90);
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
