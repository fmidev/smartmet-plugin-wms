// ======================================================================
/*!
 * \brief Layers container
 */
// ======================================================================

#pragma once

#include "ParameterInfo.h"
#include "Projection.h"
#include <boost/shared_ptr.hpp>
#include <json/json.h>
#include <list>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace HTTP
{
class Request;
}

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
  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  bool getProjection(CTPP::CDT& theGlobals,
                     CTPP::CDT& theLayersCdt,
                     State& theState,
                     Projection& projection);
  boost::optional<std::string> getProjectionParameter();
  void setProjection(Projection& projection);

  std::size_t hash_value(const State& theState) const;

  bool empty() const { return layers.empty(); }

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::list<boost::shared_ptr<Layer> > layers;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
