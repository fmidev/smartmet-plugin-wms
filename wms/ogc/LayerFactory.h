// ======================================================================
/*!
 * \brief A Web Maps Service Layer Factory data structure
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include <json/forwards.h>
#include <list>
#include <string>

namespace SmartMet::Plugin::OGC
{
class LayerConfig;
}  // namespace SmartMet::Plugin::OGC

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
class LayerFactory
{
 public:
  static std::list<SharedLayer> createLayers(const std::string& theFileName,
                                                   const std::string& theFullLayerName,
                                                   const std::string& theNamespace,
                                                   const std::string& theCustomer,
                                                   const LayerConfig& theConfig);

 private:
  static SharedLayer createLayer(Json::Value& root,
                                       const std::string& theFileName,
                                       const std::string& theFullLayerName,
                                       const std::string& theNamespace,
                                       const std::string& theCustomer,
                                       const LayerConfig& theConfig);
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
