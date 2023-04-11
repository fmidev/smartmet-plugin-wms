// ======================================================================
/*!
 * \brief A SVG graphics element layer
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "Text.h"
#include <boost/optional.hpp>
#include <string>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class TagLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  boost::optional<std::string> tag;

 private:
  boost::optional<Text> cdata;

};  // class TagLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
