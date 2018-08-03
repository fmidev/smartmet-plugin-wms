// ======================================================================
/*!
 * \brief Map layer
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "Map.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class MapLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  Map map;
  double precision = 1.0;

 private:
};  // class MapLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
