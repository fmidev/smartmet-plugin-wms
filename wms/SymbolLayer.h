// ======================================================================
/*!
 * \brief Symbol layer
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Layer.h"
#include "ParameterInfo.h"
#include "Positions.h"
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

  virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  boost::optional<std::string> parameter;
  boost::optional<double> level;

  boost::optional<Positions> positions;

  // Require at least this many valid valies
  int minvalues = 0;

  // Station distance limit in kilometers
  double maxdistance = 5;

  // the name of the default symbol
  boost::optional<std::string> symbol;
  boost::optional<double> scale;

  // optional adjustment for all symbols
  boost::optional<int> dx;
  boost::optional<int> dy;

  // The parameter values or ranges to be styled separately
  std::vector<AttributeSelection> symbols;

};  // class SymbolLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
