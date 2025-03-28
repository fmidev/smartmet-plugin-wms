#pragma once

#include "Intersections.h"
#include "Isoline.h"
#include "IsolineFilter.h"
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
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::optional<std::string> parameter;
  std::vector<Isoline> isolines;
  std::string interpolation{"linear"};
  Smoother smoother;
  int extrapolation = 0;

  double precision = 1.0;

  std::optional<double> minarea;
  std::string areaunit = "km^2";  // km^2|px^2

  std::string unit_conversion;
  std::optional<double> multiplier;
  std::optional<double> offset;

  std::optional<Map> outside;
  std::optional<Map> inside;

  Sampling sampling;
  Intersections intersections;
  IsolineFilter filter;

  bool strict = false;
  bool validate = false;
  bool desliver = false;

 protected:
  std::vector<OGRGeometryPtr> getIsolines(const std::vector<double>& isovalues, State& theState);
  Engine::Querydata::Q q;  // Make used data available to derived IsolabelLayer
  uint fileId = 0;
  uint messageIndex = 0;

 private:
  std::vector<OGRGeometryPtr> getIsolinesGrid(const std::vector<double>& isovalues,
                                              State& theState);
  std::vector<OGRGeometryPtr> getIsolinesQuerydata(const std::vector<double>& isovalues,
                                                   const State& theState);

};  // class IsolineLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
