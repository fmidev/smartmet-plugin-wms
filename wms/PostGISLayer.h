// ======================================================================
/*!
 * \brief Basic PostGIS layer
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "PostGISLayerBase.h"

#include <gis/Types.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class PostGISLayer : public PostGISLayerBase
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
