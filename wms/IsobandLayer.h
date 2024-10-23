// ======================================================================
/*!
 * \brief Isoband layer
 */
// ======================================================================

#pragma once

#include "Heatmap.h"
#include "Intersections.h"
#include "Isoband.h"
#include "IsolineFilter.h"
#include "Layer.h"
#include "Map.h"
#include "ParameterInfo.h"
#include "Sampling.h"
#include "Smoother.h"
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

class IsobandLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::optional<std::string> parameter;
  std::vector<Isoband> isobands;
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

  Heatmap heatmap;

  bool closed_range = true;
  bool strict = false;
  bool validate = false;
  bool desliver = false;

 private:
  virtual void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  virtual void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  std::shared_ptr<Engine::Querydata::QImpl> buildHeatmap(const Spine::Parameter& theParameter,
                                                         const Fmi::DateTime& theTime,
                                                         State& theState);

};  // class IsobandLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
