// ======================================================================
/*!
 * \brief Base for PostGIS-related layers
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "PostGISLayerFilter.h"
#include <engines/gis/MapOptions.h>
#include <gis/Types.h>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

// Explain an empty map fetch. The geometry can be missing either because the
// query matched no rows or because the configured filters removed everything
// that was returned -- the GIS engine reports both as a null geometry. A read
// that actually failed throws from Fmi::PostGIS::read() instead of looking
// empty, so it never reaches here. Report the filters that are in effect rather
// than blaming minarea unconditionally.
std::string emptyMapMessage(const Engine::Gis::MapOptions& theMapOptions);

class PostGISLayerBase : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override = 0;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override = 0;

  std::size_t hash_value(const State& theState) const override;

  bool isLines() const { return lines; }

 protected:
  bool lines = false;  // polygon clipping by default
  std::string pgname;
  std::string schema;
  std::string table;

  double precision = 1.0;
  std::optional<std::string> time_column;     // Needed for GetCapabilities
  std::optional<std::string> time_condition;  // SQL for temporal selection
  std::vector<PostGISLayerFilter> filters;

  static OGRGeometryPtr getShape(const State& theState,
                                 const Fmi::SpatialReference& theSR,
                                 Engine::Gis::MapOptions& theMapOptions);

  static Fmi::Features getFeatures(const State& theState,
                                   const Fmi::SpatialReference& theSR,
                                   Engine::Gis::MapOptions& theMapOptions);

};  // class PostGISLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
