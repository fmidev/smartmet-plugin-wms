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
#include "StreamLayer.h"
#include "SymbolLayer.h"
#include "TagLayer.h"
#include "TimeLayer.h"
#include "TranslationLayer.h"
#include "WKTLayer.h"
#include "WeatherObjectsLayer.h"
#include "WindRoseLayer.h"
#include <macgyver/Exception.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace LayerFactory
{
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

    if (name == "arrow")
      return new ArrowLayer;
    if (name == "background")
      return new BackgroundLayer;
    if (name == "group")
      return new GroupLayer;
    if (name == "postgis")
      return new PostGISLayer;
    if (name == "isoband")
      return new IsobandLayer;
    if (name == "isoline")
      return new IsolineLayer;
    if (name == "isolabel")
      return new IsolabelLayer;
    if (name == "legend")
      return new LegendLayer;
    if (name == "location")
      return new LocationLayer;
    if (name == "map")
      return new MapLayer;
    if (name == "null")
      return new NullLayer;
#ifndef WITHOUT_OSM
    if (name == "osm")
      return new OSMLayer;
#endif
    if (name == "number")
      return new NumberLayer;
    if (name == "raster")
      return new RasterLayer;
    if (name == "geotiff")
      return new GeoTiffLayer;
    if (name == "hovmoeller")
      return new HovmoellerLayer;
    if (name == "stream")
      return new StreamLayer;
    if (name == "symbol")
      return new SymbolLayer;
    if (name == "tag")
      return new TagLayer;
    if (name == "time")
      return new TimeLayer;
    if (name == "translation")
      return new TranslationLayer;
    if (name == "wkt")
      return new WKTLayer;
#ifndef WITHOUT_OBSERVATION
    if (name == "windrose")
      return new WindRoseLayer;
    if (name == "finnish_road_observation")
      return new FinnishRoadObservationLayer;
    if (name == "present_weather_observation")
      return new PresentWeatherObservationLayer;
    if (name == "cloud_ceiling")
      return new CloudCeilingLayer;
#endif
    if (name == "icemap")
      return new IceMapLayer;
    if (name == "frame")
      return new FrameLayer;
    if (name == "fronts")
      return new WeatherFrontsLayer;
    if (name == "weather_objects")
      return new WeatherObjectsLayer;
#ifndef WITHOUT_AVI
    if (name == "metar")
      return new MetarLayer;
#endif
    if (name == "grid")
      return new GridLayer;
    if (name == "circle")
      return new CircleLayer;
    if (name == "graticule")
      return new GraticuleLayer;

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
