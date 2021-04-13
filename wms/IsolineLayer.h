#pragma once

#include "Intersections.h"
#include "Isoline.h"
#include "Layer.h"
#include "Map.h"
#include "ParameterInfo.h"
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

class IsolineLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  boost::optional<std::string> parameter;
  std::vector<Isoline> isolines;
  Smoother smoother;
  int extrapolation = 0;

  double precision = 1.0;

  boost::optional<double> minarea;

  std::string unit_conversion;
  boost::optional<double> multiplier;
  boost::optional<double> offset;

  boost::optional<Map> outside;
  boost::optional<Map> inside;

  Sampling sampling;
  Intersections intersections;

 protected:
  std::vector<OGRGeometryPtr> getIsolines(const std::vector<double> isovalues, State& theState);
  Engine::Querydata::Q q;  // Make used data available to derived IsolabelLayer
  uint fileId = 0;
  uint messageIndex = 0;

 private:
  std::vector<OGRGeometryPtr> getIsolinesGrid(const std::vector<double> isovalues, State& theState);
  std::vector<OGRGeometryPtr> getIsolinesQuerydata(const std::vector<double> isovalues,
                                                   State& theState);

};  // class IsolineLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
