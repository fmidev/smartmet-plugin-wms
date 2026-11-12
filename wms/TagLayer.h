// ======================================================================
/*!
 * \brief A SVG graphics element layer
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include "Text.h"
#include <cstddef>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class State;

class TagLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  std::optional<std::string> tag;

 private:
  void resolveEdgeRelativePosition(const State& theState);
  void positionSublayers(const std::string& longitude, const std::string& latitude);

  std::optional<Text> cdata;

};  // class TagLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
