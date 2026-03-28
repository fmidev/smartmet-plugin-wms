// ======================================================================
/*!
 * \brief A Meb Maps Service Layer Factory data structure
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "Layer.h"
#include <engines/querydata/Engine.h>
#include <spine/Json.h>
#include <list>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class LayerFactory
{
 public:
  static std::list<SharedLayer> createLayers(const std::string& theFileName,
                                                   const std::string& theFullLayerName,
                                                   const std::string& theNamespace,
                                                   const std::string& theCustomer,
                                                   const Config& theConfig);

 private:
  static SharedLayer createLayer(Json::Value& root,
                                       const std::string& theFileName,
                                       const std::string& theFullLayerName,
                                       const std::string& theNamespace,
                                       const std::string& theCustomer,
                                       const Config& theConfig);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
