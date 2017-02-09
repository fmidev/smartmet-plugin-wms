// ======================================================================
/*!
 * \brief Symbol layer
 */
// ======================================================================

#pragma once

#include "Positions.h"
#include "Layer.h"
#include "AttributeSelection.h"
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
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  boost::optional<std::string> parameter;
  boost::optional<double> level;

  Positions positions;

  // the name of the default symbol
  boost::optional<std::string> symbol;
  boost::optional<double> scale;

  // The parameter values or ranges to be styled separately
  std::vector<AttributeSelection> symbols;

 private:
};  // class SymbolLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
