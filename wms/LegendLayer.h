// ======================================================================
/*!
 * \brief Legend layer
 */
// ======================================================================

#pragma once

#include "Isoband.h"
#include "Layer.h"
#include "LegendLabels.h"
#include "LegendSymbols.h"
#include <boost/optional.hpp>
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
  void init(const Json::Value& theJson,
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
};  // class LegendLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
