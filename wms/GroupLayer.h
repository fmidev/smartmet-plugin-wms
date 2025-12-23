// ======================================================================
/*!
 * \brief A background layer to fill the view
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "Projection.h"
#include <optional>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class GroupLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  int compression;

 private:
};  // class GroupLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
