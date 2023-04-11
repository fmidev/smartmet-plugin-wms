// ======================================================================
/*!
 * \brief A dummy layer to make style additions easier
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"

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

class NullLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

};  // class TagLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
