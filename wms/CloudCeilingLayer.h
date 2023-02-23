/*!
 * \brief CloudCeilingLayer layer
 */
// ======================================================================

#ifndef WITHOUT_OBSERVATION

#pragma once

#include <engines/observation/Engine.h>

#include "NumberLayer.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class CloudCeilingLayer : public NumberLayer
{
 public:
  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;


  SmartMet::Spine::TaggedFMISIDList stationFMISIDs;
};  // class CloudCeilingLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
