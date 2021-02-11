#ifdef WGS84

// ======================================================================
/*!
 * \brief Grid layer
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

class GridLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

};  // class GridLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif
