// ======================================================================
/*!
 * \brief Symbol layer
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Layer.h"
#include "ParameterInfo.h"
#include "PointValueOptions.h"
#include "Positions.h"
#include <timeseries/ParameterFactory.h>
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

class SymbolLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::optional<TS::ParameterAndFunctions> param_funcs;
  std::optional<std::string> parameter;
  std::string unit_conversion;
  std::optional<double> multiplier;
  std::optional<double> offset;

  std::optional<Positions> positions;

  // Require at least this many valid valies
  int minvalues = 0;

  // Station distance limit in kilometers
  double maxdistance = 5;

  // the name of the default symbol
  std::optional<std::string> symbol;
  std::optional<double> scale;

  // optional adjustment for all symbols
  std::optional<int> dx;
  std::optional<int> dy;

  // The parameter values or ranges to be styled separately
  std::vector<AttributeSelection> symbols;

  // Minimum distance, priorities of symbols
  PointValueOptions point_value_options;

 private:
  virtual void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  virtual void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

};  // class SymbolLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
