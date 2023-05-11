// ======================================================================
/*!
 * \brief Arrow layer with possible speed range styling
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Layer.h"
#include "ParameterInfo.h"
#include "PointValueOptions.h"
#include "Positions.h"
#include "Projection.h"
#include <boost/optional.hpp>
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

class ArrowLayer : public Layer
{
 public:
  ArrowLayer() = default;
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  // Direction and speed parameters
  boost::optional<std::string> direction;
  boost::optional<std::string> speed;
  // U- and V-components, mutually exclusive with above settings
  boost::optional<std::string> u;
  boost::optional<std::string> v;

  boost::optional<double> fixedspeed;
  boost::optional<double> fixeddirection;

  std::string unit_conversion;
  boost::optional<double> multiplier;
  boost::optional<double> offset;
  boost::optional<double> minrotationspeed;
  boost::optional<std::string> symbol;
  boost::optional<double> scale;
  bool southflop = false;
  bool northflop = false;
  bool flip = false;

  // Arrow position generator
  boost::optional<Positions> positions;
  boost::optional<int> dx;
  boost::optional<int> dy;

  // Require at least this many valid valies
  int minvalues = 0;

  // Station distance limit in kilometers
  double maxdistance = 5;

  // The speed parameter and the ranges to be styled separately
  std::vector<AttributeSelection> arrows;

  // Minimum distance, priorities of arrows
  PointValueOptions point_value_options;

 private:
  void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
};  // class ArrowLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
