// ======================================================================
/*!
 * \brief Location layer
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Layer.h"
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

class LocationLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  std::size_t hash_value(const State& theState) const override;

  std::string keyword;
  double mindistance = 30;

  std::set<std::string> countries;

  // the name of the default symbol
  boost::optional<std::string> symbol;

  // Feature code dependent symbol and attribute selection
  std::map<std::string, std::vector<AttributeSelection> > symbols;

 private:
};  // class LocationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
