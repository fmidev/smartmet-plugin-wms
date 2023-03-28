#pragma once

#include "Intersections.h"
#include "Isoline.h"
#include "Layer.h"
#include "Map.h"
#include "ParameterInfo.h"
#include "Positions.h"
#include "Sampling.h"
#include "Smoother.h"
#include <engines/querydata/Q.h>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class StreamLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  boost::optional<std::string> parameter;
  boost::optional<std::string> u_parameter;
  boost::optional<std::string> v_parameter;
  int minStreamLen = 5;
  int maxStreamLen = 2048;
  int lineLen = 32;
  int xStep = 20;
  int yStep = 20;
  double precision = 1.0;

 protected:
  std::vector<OGRGeometryPtr> getStreams(State& theState);
  Engine::Querydata::Q q;  // Make used data available to derived IsolabelLayer
  uint fileId = 0;
  uint messageIndex = 0;

 private:
  std::vector<OGRGeometryPtr> getStreamsGrid(State& theState);
  std::vector<OGRGeometryPtr> getStreamsQuerydata(const State& theState);

};  // class StreamLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
