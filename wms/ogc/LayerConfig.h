// ======================================================================
/*!
 * \brief OGC Layer configuration - shared infrastructure for WMS/WMTS etc.
 *
 * Stores raw pointers to engines and pointer-to-const for Dali::Config
 * and Spine::JsonCache so the class is copyable/assignable.
 */
// ======================================================================

#pragma once

#include "LegendGraphicSettings.h"
#include "SupportedReference.h"
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/querydata/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <spine/JsonCache.h>
#include <map>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
}

namespace OGC
{
class LayerConfig
{
 public:
  LayerConfig() = default;

  LayerConfig& setDaliConfig(const Dali::Config& c)
  {
    itsDaliConfig = &c;
    return *this;
  }
  LayerConfig& setJsonCache(const Spine::JsonCache& c)
  {
    itsJsonCache = &c;
    return *this;
  }
  LayerConfig& setLegendGraphicSettings(const LegendGraphicSettings& s)
  {
    itsLegendGraphicSettings = s;
    return *this;
  }
  LayerConfig& setQEngine(Engine::Querydata::Engine* e)
  {
    itsQEngine = e;
    return *this;
  }
  LayerConfig& setGisEngine(Engine::Gis::Engine* e)
  {
    itsGisEngine = e;
    return *this;
  }
  LayerConfig& setGridEngine(Engine::Grid::Engine* e)
  {
    itsGridEngine = e;
    return *this;
  }
  LayerConfig& setObservationProducers(std::set<std::string> p)
  {
    itsObservationProducers = std::move(p);
    return *this;
  }
  LayerConfig& setSupportedReferences(std::map<std::string, SupportedReference> refs)
  {
    itsSupportedReferences = std::move(refs);
    return *this;
  }
#ifndef WITHOUT_OBSERVATION
  LayerConfig& setObsEngine(Engine::Observation::Engine* e)
  {
    itsObsEngine = e;
    return *this;
  }
#endif

  const Dali::Config& getDaliConfig() const { return *itsDaliConfig; }
  const Spine::JsonCache& getJsonCache() const { return *itsJsonCache; }
  const LegendGraphicSettings& getLegendGraphicSettings() const
  {
    return itsLegendGraphicSettings;
  }
  Engine::Querydata::Engine* qEngine() const { return itsQEngine; }
  Engine::Gis::Engine* gisEngine() const { return itsGisEngine; }
  Engine::Grid::Engine* gridEngine() const { return itsGridEngine; }
  const std::set<std::string>& getObservationProducers() const { return itsObservationProducers; }
  const std::map<std::string, SupportedReference>& getSupportedReferences() const
  {
    return itsSupportedReferences;
  }
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* obsEngine() const { return itsObsEngine; }
#endif

 private:
  const Dali::Config* itsDaliConfig = nullptr;
  const Spine::JsonCache* itsJsonCache = nullptr;
  LegendGraphicSettings itsLegendGraphicSettings;
  Engine::Querydata::Engine* itsQEngine = nullptr;
  Engine::Gis::Engine* itsGisEngine = nullptr;
  Engine::Grid::Engine* itsGridEngine = nullptr;
  std::set<std::string> itsObservationProducers;
  std::map<std::string, SupportedReference> itsSupportedReferences;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* itsObsEngine = nullptr;
#endif
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
