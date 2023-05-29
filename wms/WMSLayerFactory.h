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

#include "WMSConfig.h"
#include "WMSLayer.h"
#include <engines/querydata/Engine.h>
#include <spine/Json.h>
#include <list>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSLayerFactory
{
 public:
  static std::list<SharedWMSLayer> createWMSLayers(const std::string& theFileName,
                                                   const std::string& theFullLayerName,
                                                   const std::string& theNamespace,
                                                   const std::string& theCustomer,
                                                   const WMSConfig& theWMSConfig);

 private:
  static SharedWMSLayer createWMSLayer(Json::Value& root,
                                       const std::string& theFileName,
                                       const std::string& theFullLayerName,
                                       const std::string& theNamespace,
                                       const std::string& theCustomer,
                                       const WMSConfig& theWMSConfig);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
