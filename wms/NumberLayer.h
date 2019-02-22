// ======================================================================
/*!
 * \brief Number layer
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Label.h"
#include "Layer.h"
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

class NumberLayer : public Layer
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
  std::string unit_conversion;
  boost::optional<double> multiplier;
  boost::optional<double> offset;

  // Grid coordinate settings
  boost::optional<Positions> positions;

  // Station distance limit in kilometers
  double maxdistance = 5;

  // Label settings
  Label label;

  // default symbol
  boost::optional<std::string> symbol;
  boost::optional<double> scale;

  std::vector<AttributeSelection> numbers;

 private:
};  // class NumberLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
