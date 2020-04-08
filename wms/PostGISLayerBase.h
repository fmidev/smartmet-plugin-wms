// ======================================================================
/*!
 * \brief Base for PostGIS-related layers
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "PostGISLayerFilter.h"
#include <boost/optional.hpp>
#include <engines/gis/MapOptions.h>
#include <gis/Types.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class PostGISLayerBase : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) = 0;

  virtual std::size_t hash_value(const State& theState) const;

  bool isLines() const { return lines; }

 protected:
  bool lines = false;  // polygon clipping by default
  std::string pgname;
  std::string schema;
  std::string table;
  double precision = 1.0;
  boost::optional<std::string> time_column;     // Needed for GetCapabilities
  boost::optional<std::string> time_condition;  // SQL for temporal selection
  std::vector<PostGISLayerFilter> filters;

  OGRGeometryPtr getShape(const State& theState,
                          const Fmi::SpatialReference& theSR,
                          Engine::Gis::MapOptions& theMapOptions) const;

  Fmi::Features getFeatures(const State& theState,
                            const Fmi::SpatialReference& theSR,
                            Engine::Gis::MapOptions& theMapOptions) const;

};  // class PostGISLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
