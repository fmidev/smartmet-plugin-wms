// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "WMSLayer.h"
#include <boost/shared_ptr.hpp>
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
// For getCapabilities caching. Note that all returned objects must be copies for
// thread safety reasons - there is a loop which updates layer proxies on the
// background and individual proxies may be destroyed.

//
class WMSLayerProxy
{
 public:
  WMSLayerProxy(Engine::Gis::Engine* gisEngine, SharedWMSLayer theLayer)
      : itsGisEngine(gisEngine), itsLayer(theLayer)
  {
    itsCapabilities = itsLayer->generateGetCapabilities(*itsGisEngine);
  }

  SharedWMSLayer getLayer() const { return itsLayer; }
  boost::shared_ptr<CTPP::CDT> getCapabilities() const { return itsCapabilities; }

  void update()
  {
    itsLayer->updateLayerMetaData();
    itsCapabilities = itsLayer->generateGetCapabilities(*itsGisEngine);
  }

 private:
  const Engine::Gis::Engine* itsGisEngine;
  SharedWMSLayer itsLayer;
  boost::shared_ptr<CTPP::CDT> itsCapabilities;  // empty implies hidden layer
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
