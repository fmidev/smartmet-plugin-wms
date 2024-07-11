// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "WMSLayer.h"
#include <memory>
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
      : itsGisEngine(gisEngine), itsLayer(std::move(theLayer))
  {
  }

  WMSLayerProxy(const WMSLayerProxy& other) = default;
  WMSLayerProxy& operator=(const WMSLayerProxy& other) = default;

  SharedWMSLayer getLayer() const { return itsLayer; }

  std::optional<CTPP::CDT> getCapabilities(
      bool multiple_intervals,
	  bool show_hidden,
      const std::string& language,
      const std::optional<std::string>& starttime,
      const std::optional<std::string>& endtime,
      const std::optional<std::string>& reference_time) const
  {
    return itsLayer->generateGetCapabilities(multiple_intervals, show_hidden, *itsGisEngine, language, starttime, endtime, reference_time);
  }

 private:
  const Engine::Gis::Engine* itsGisEngine;
  SharedWMSLayer itsLayer;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
