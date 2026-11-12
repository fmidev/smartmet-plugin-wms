// ======================================================================
/*!
 * \brief A SVG text element with translations
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include <cstddef>
#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class TranslationLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  std::string tag = "text";
  std::map<std::string, std::string> translations;

 private:
};  // class TranslationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
