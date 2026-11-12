// ======================================================================
/*!
 * \brief Grid layer
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include <cstddef>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class GridLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

};  // class GridLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
