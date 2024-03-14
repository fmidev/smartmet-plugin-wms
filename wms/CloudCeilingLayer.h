/*!
 * \brief CloudCeilingLayer layer
 */
// ======================================================================

#ifndef WITHOUT_OBSERVATION

#pragma once

#include "NumberLayer.h"
#include <engines/observation/Engine.h>

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

  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  //  SmartMet::Spine::TaggedFMISIDList stationFMISIDs;

 private:
  std::vector<PointData> readObservations(
      State& state,
      const Fmi::SpatialReference& crs,
      const Fmi::Box& box,
      const Fmi::DateTime& valid_time,
      const Fmi::TimePeriod& valid_time_period) const;

  std::string itsKeyword;
  std::vector<int> itsFMISIDs;
};  // class CloudCeilingLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
