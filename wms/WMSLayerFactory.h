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

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSLayerFactory
{
 public:
  static SharedWMSLayer createWMSLayer(const std::string& theFileName,
                                       const std::string& theNamespace,
                                       const std::string& theCustomer,
                                       const WMSConfig& theWMSConfig);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
