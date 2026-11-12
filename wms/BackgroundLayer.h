// ======================================================================
/*!
 * \brief A background layer to fill the view
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

class BackgroundLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
};  // class BackgroundLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
