// ======================================================================
/*!
 * \brief Isoband layer
 */
// ======================================================================

#pragma once

#include "Heatmap.h"
#include "Intersections.h"
#include "Isoband.h"
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
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  boost::optional<std::string> parameter;
  boost::optional<double> level;
  std::vector<Isoband> isobands;
  std::string interpolation{"linear"};
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

  Heatmap heatmap;

 private:
  boost::shared_ptr<Engine::Querydata::QImpl> buildHeatmap(const Spine::Parameter& theParameter,
                                                           const boost::posix_time::ptime& theTime,
                                                           State& theState);

};  // class IsobandLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
