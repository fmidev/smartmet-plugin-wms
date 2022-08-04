// ======================================================================
/*!
 * \brief Map layer
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "Map.h"
#include "MapStyles.h"
#include <boost/optional.hpp>

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
  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  Map map;
  double precision = 1.0;

  boost::optional<MapStyles> styles;

 private:
  void generate_full_map(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  void generate_styled_map(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

};  // class MapLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
