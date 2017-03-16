// ======================================================================

#include "LayerFactory.h"
#include "ArrowLayer.h"
#include "BackgroundLayer.h"
#include "PostGISLayer.h"
#include "IsolineLayer.h"
#include "IsobandLayer.h"
#include "LegendLayer.h"
#include "LocationLayer.h"
#include "MapLayer.h"
#include "NumberLayer.h"
#include "SymbolLayer.h"
#include "TagLayer.h"
#include "TimeLayer.h"
#include "TranslationLayer.h"
#include "WindRoseLayer.h"
#include "WKTLayer.h"
#include "IceMapLayer.h"
#include <stdexcept>
#include <spine/Exception.h>

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
      throw Spine::Exception(BCP, "Layer JSON must be an object");

    auto name = theJson.get("layer_type", "tag").asString();

    if (name == "arrow")
      return new ArrowLayer;
    else if (name == "background")
      return new BackgroundLayer;
    else if (name == "postgis")
      return new PostGISLayer;
    else if (name == "isoband")
      return new IsobandLayer;
    else if (name == "isoline")
      return new IsolineLayer;
    else if (name == "legend")
      return new LegendLayer;
    else if (name == "location")
      return new LocationLayer;
    else if (name == "map")
      return new MapLayer;
    else if (name == "number")
      return new NumberLayer;
    else if (name == "symbol")
      return new SymbolLayer;
    else if (name == "tag")
      return new TagLayer;
    else if (name == "time")
      return new TimeLayer;
    else if (name == "translation")
      return new TranslationLayer;
    else if (name == "wkt")
      return new WKTLayer;
#ifndef WITHOUT_OBSERVATION
    else if (name == "windrose")
      return new WindRoseLayer;
#endif
    else if (name == "icemap")
      return new IceMapLayer;
    else
      throw Spine::Exception(BCP, "Unknown layer type '" + name + "'");
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}  // namespace LayerFactory
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
