// ======================================================================

#include "LayerFactory.h"
#include "ArrowLayer.h"
#include "BackgroundLayer.h"
#include "CircleLayer.h"
#include "CloudCeilingLayer.h"
#include "FinnishRoadObservationLayer.h"
#include "FrameLayer.h"
#include "WeatherFrontsLayer.h"
#include "GraticuleLayer.h"
#include "GridLayer.h"
#include "GroupLayer.h"
#include "IceMapLayer.h"
#include "IsobandLayer.h"
#include "IsolabelLayer.h"
#include "IsolineLayer.h"
#include "LegendLayer.h"
#include "LocationLayer.h"
#include "MapLayer.h"
#include "NullLayer.h"
#include "NumberLayer.h"
#ifndef WITHOUT_OSM
#include "OSMLayer.h"
#endif
#include "PostGISLayer.h"
#include "PresentWeatherObservationLayer.h"
#include "GeoTiffLayer.h"
#include "HovmoellerLayer.h"
#include "MetarLayer.h"
#include "RasterLayer.h"
#include "SatelliteLayer.h"
#include "StreamLayer.h"
#include "SymbolLayer.h"
#include "TagLayer.h"
#include "TimeLayer.h"
#include "TranslationLayer.h"
#include "WKTLayer.h"
#include "WeatherObjectsLayer.h"
#include "WindRoseLayer.h"
#include <macgyver/Exception.h>
#include <map>
#include <stdexcept>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace LayerFactory
{
namespace
{
template <typename T>
Layer* make_layer()
{
  return new T;
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Create a new layer
 *
 * This should return std::unique_ptr
 */
//----------------------------------------------------------------------

Layer* create(const Json::Value& theJson)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Layer JSON must be an object");

    auto name = theJson.get("layer_type", "tag").asString();

    using LayerCreator = Layer* (*)();
    static const std::map<std::string, LayerCreator> creators{
        {"arrow", make_layer<ArrowLayer>},
        {"background", make_layer<BackgroundLayer>},
        {"group", make_layer<GroupLayer>},
        {"postgis", make_layer<PostGISLayer>},
        {"isoband", make_layer<IsobandLayer>},
        {"isoline", make_layer<IsolineLayer>},
        {"isolabel", make_layer<IsolabelLayer>},
        {"legend", make_layer<LegendLayer>},
        {"location", make_layer<LocationLayer>},
        {"map", make_layer<MapLayer>},
        {"null", make_layer<NullLayer>},
#ifndef WITHOUT_OSM
        {"osm", make_layer<OSMLayer>},
#endif
        {"number", make_layer<NumberLayer>},
        {"raster", make_layer<RasterLayer>},
        {"satellite", make_layer<SatelliteLayer>},
        {"geotiff", make_layer<GeoTiffLayer>},
        {"hovmoeller", make_layer<HovmoellerLayer>},
        {"stream", make_layer<StreamLayer>},
        {"symbol", make_layer<SymbolLayer>},
        {"tag", make_layer<TagLayer>},
        {"time", make_layer<TimeLayer>},
        {"translation", make_layer<TranslationLayer>},
        {"wkt", make_layer<WKTLayer>},
#ifndef WITHOUT_OBSERVATION
        {"windrose", make_layer<WindRoseLayer>},
        {"finnish_road_observation", make_layer<FinnishRoadObservationLayer>},
        {"present_weather_observation", make_layer<PresentWeatherObservationLayer>},
        {"cloud_ceiling", make_layer<CloudCeilingLayer>},
#endif
        {"icemap", make_layer<IceMapLayer>},
        {"frame", make_layer<FrameLayer>},
        {"fronts", make_layer<WeatherFrontsLayer>},
        {"weather_objects", make_layer<WeatherObjectsLayer>},
#ifndef WITHOUT_AVI
        {"metar", make_layer<MetarLayer>},
#endif
        {"grid", make_layer<GridLayer>},
        {"circle", make_layer<CircleLayer>},
        {"graticule", make_layer<GraticuleLayer>},
    };

    auto pos = creators.find(name);
    if (pos != creators.end())
      return pos->second();

    throw Fmi::Exception(BCP, "Unknown layer type '" + name + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace LayerFactory
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
