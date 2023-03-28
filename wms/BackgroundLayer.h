// ======================================================================
/*!
 * \brief A background layer to fill the view
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "Projection.h"
#include <boost/optional.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class BackgroundLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
};  // class BackgroundLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
