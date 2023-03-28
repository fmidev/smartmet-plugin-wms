// ======================================================================

#include "LayerFactory.h"
#include "ArrowLayer.h"
#include "BackgroundLayer.h"
#include "CloudCeilingLayer.h"
#include "FinnishRoadObservationLayer.h"
#include "FrameLayer.h"
#include "GridLayer.h"
#include "IceMapLayer.h"
#include "IsobandLayer.h"
#include "IsolabelLayer.h"
#include "IsolineLayer.h"
#include "LegendLayer.h"
#include "LocationLayer.h"
#include "MapLayer.h"
#include "NumberLayer.h"
#include "PostGISLayer.h"
#include "PresentWeatherObservationLayer.h"
#include "StreamLayer.h"
#include "SymbolLayer.h"
#include "TagLayer.h"
#include "TimeLayer.h"
#include "TranslationLayer.h"
#include "WKTLayer.h"
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
    if (name == "number")
      return new NumberLayer;
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
    if (name == "grid")
      return new GridLayer;

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
