// ======================================================================
/*!
 * \brief Location layer
 */
// ======================================================================

#pragma once

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

class LocationLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  virtual std::size_t hash_value(const State& theState) const;

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
