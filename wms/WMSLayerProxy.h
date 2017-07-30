// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "WMSLayer.h"
#include <ctpp2/CDT.hpp>
#include <string>

namespace SmartMet
{
namespace Engine
{
namespace Gis
{
class Engine;
}
}

namespace Plugin
{
namespace WMS
{
// For getCapabilities caching
class WMSLayerProxy
{
 public:
  WMSLayerProxy(Engine::Gis::Engine* gisEngine, SharedWMSLayer theLayer)
      : itsGisEngine(gisEngine), itsLayer(theLayer)
  {
    itsCapabilities = itsLayer->generateGetCapabilities(*itsGisEngine);
  }

  SharedWMSLayer getLayer() const { return itsLayer; }
  const boost::optional<CTPP::CDT>& getCapabilities() const { return itsCapabilities; }

  void update()
  {
    itsLayer->updateLayerMetaData();
    itsCapabilities = CTPP::CDT(CTPP::CDT::HASH_VAL);
    itsCapabilities = itsLayer->generateGetCapabilities(*itsGisEngine);
  }

 private:
  const Engine::Gis::Engine* itsGisEngine;
  SharedWMSLayer itsLayer;
  boost::optional<CTPP::CDT> itsCapabilities;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
