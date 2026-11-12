// ======================================================================
/*!
 * \brief Basic PostGIS layer
 */
// ======================================================================

#pragma once

#include "PostGISLayerBase.h"
#include <cstddef>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class PostGISLayer : public PostGISLayerBase
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
