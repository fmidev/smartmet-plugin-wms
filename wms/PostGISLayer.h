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
  PostGISLayer() : PostGISLayerBase() {}
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
