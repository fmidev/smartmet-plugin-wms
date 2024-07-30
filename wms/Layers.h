// ======================================================================
/*!
 * \brief Layers container
 */
// ======================================================================

#pragma once

#include "ParameterInfo.h"
#include "Projection.h"
#include "Warnings.h"
#include <json/json.h>
#include <list>
#include <memory>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Properties;
class State;
class Layer;

class Layers
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  void check_warnings(Warnings& warnings) const;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  bool getProjection(CTPP::CDT& theGlobals,
                     CTPP::CDT& theLayersCdt,
                     State& theState,
                     Projection& projection);
  std::optional<std::string> getProjectionParameter();
  void setProjection(const Projection& projection);

  std::size_t hash_value(const State& theState) const;

  bool empty() const { return layers.empty(); }

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::list<std::shared_ptr<Layer> > layers;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
