// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include <ctpp2/CDT.hpp>
#include <memory>
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
namespace OGC
{
class LayerProxy
{
 public:
  LayerProxy(Engine::Gis::Engine* gisEngine, SharedLayer theLayer)
      : itsGisEngine(gisEngine), itsLayer(std::move(theLayer))
  {
  }

  LayerProxy(const LayerProxy& other) = default;
  LayerProxy& operator=(const LayerProxy& other) = default;

  SharedLayer getLayer() const { return itsLayer; }

  std::optional<CTPP::CDT> getCapabilities(bool multiple_intervals,
                                           bool show_hidden,
                                           const std::string& language,
                                           const std::string& defaultLanguage,
                                           const std::optional<Fmi::DateTime>& starttime,
                                           const std::optional<Fmi::DateTime>& endtime,
                                           const std::optional<Fmi::DateTime>& reference_time) const
  {
    return itsLayer->generateGetCapabilities(multiple_intervals,
                                             show_hidden,
                                             *itsGisEngine,
                                             language,
                                             defaultLanguage,
                                             starttime,
                                             endtime,
                                             reference_time);
  }

 private:
  const Engine::Gis::Engine* itsGisEngine;
  SharedLayer itsLayer;
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
