// ======================================================================
/*!
 * \brief Number layer
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Label.h"
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

class NumberLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::optional<TS::ParameterAndFunctions> param_funcs;
  std::optional<std::string> parameter;
  std::string unit_conversion;
  std::optional<double> multiplier;
  std::optional<double> offset;

  // Grid coordinate settings
  std::optional<Positions> positions;

  // Require at least this many valid valies
  int minvalues = 0;

  // Station distance limit in kilometers
  double maxdistance = 5;

  // Minimum distance, priorities of numbers
  PointValueOptions point_value_options;

  // Label settings
  Label label;

  // default symbol
  std::optional<std::string> symbol;
  std::optional<double> scale;  // has no effect on numbers, only on the symbol

  std::vector<AttributeSelection> numbers;

 private:
  virtual void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  virtual void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

};  // class NumberLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
