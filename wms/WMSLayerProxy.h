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
}  // namespace Engine

namespace Plugin
{
namespace WMS
{
class WMSLayerProxy
{
 public:
  WMSLayerProxy(Engine::Gis::Engine* gisEngine, SharedWMSLayer theLayer)
      : itsGisEngine(gisEngine), itsLayer(theLayer)
  {
  }

  SharedWMSLayer getLayer() const { return itsLayer; }

  boost::optional<CTPP::CDT> getCapabilities(
      bool multiple_intervals,
      const boost::optional<std::string>& starttime,
      const boost::optional<std::string>& endtime,
      const boost::optional<std::string>& reference_time) const
  {
    return itsLayer->generateGetCapabilities(
        multiple_intervals, *itsGisEngine, starttime, endtime, reference_time);
  }

 private:
  const Engine::Gis::Engine* itsGisEngine;
  SharedWMSLayer itsLayer;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
