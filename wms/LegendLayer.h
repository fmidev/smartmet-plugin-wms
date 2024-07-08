// ======================================================================
/*!
 * \brief Legend layer
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Isoband.h"
#include "Layer.h"
#include "LegendLabels.h"
#include "LegendSymbols.h"
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class LegendLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  int x = 10;
  int y = 10;
  int dx = 0;
  int dy = 20;

  LegendSymbols symbols;
  LegendLabels labels;
  std::vector<Isoband> isobands;

 private:
  // The parameter values or ranges to be styled separately
  std::vector<AttributeSelection> symbol_vector;
  void generate_from_symbol_vector(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
};  // class LegendLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
