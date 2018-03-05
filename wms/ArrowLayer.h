// ======================================================================
/*!
 * \brief Arrow layer with possible speed range styling
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Layer.h"
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
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  // Direction and speed parameters
  boost::optional<std::string> direction;
  boost::optional<std::string> speed;
  // U- and V-components, mutually exclusive with above settings
  boost::optional<std::string> u;
  boost::optional<std::string> v;

  boost::optional<double> level;
  boost::optional<double> multiplier;
  boost::optional<double> offset;
  boost::optional<std::string> symbol;
  boost::optional<double> scale;
  bool southflop;

  // Arrow position generator
  boost::optional<Positions> positions;
  boost::optional<int> dx;
  boost::optional<int> dy;

  // Station distance limit in kilometers
  double maxdistance = 5;

  // The speed parameter and the ranges to be styled separately
  std::vector<AttributeSelection> arrows;

 private:
};  // class ArrowLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
