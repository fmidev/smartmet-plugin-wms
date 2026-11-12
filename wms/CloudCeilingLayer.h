/*!
 * \brief CloudCeilingLayer layer
 */
// ======================================================================

#ifndef WITHOUT_OBSERVATION

#pragma once

#include "NumberLayer.h"
#include "PointData.h"
#include <gis/SpatialReference.h>
#include <macgyver/DateTime.h>
#include <timeseries/TimeSeries.h>
#include <timeseries/TimeSeriesInclude.h>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Fmi
{
class Box;
class CoordinateTransformation;
}  // namespace Fmi
namespace SmartMet::Engine::Observation
{
class Settings;
}  // namespace SmartMet::Engine::Observation
namespace SmartMet::Plugin::Dali
{
class State;
}  // namespace SmartMet::Plugin::Dali

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
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  //  SmartMet::Spine::TaggedFMISIDList stationFMISIDs;

 protected:
  // The cloud layer and height parameters are fixed
  bool requiresParameter() const override { return false; }

 private:
  void addStations(Engine::Observation::Settings& settings,
                   const State& state,
                   const Fmi::SpatialReference& crs,
                   const Fmi::Box& box) const;
  std::optional<PointData> ceilingPoint(const TS::TimeSeriesVector& values,
                                        std::size_t row,
                                        double lon,
                                        double lat,
                                        const Fmi::SpatialReference& crs,
                                        const Fmi::CoordinateTransformation& transformation,
                                        const Fmi::Box& box) const;
  std::vector<PointData> readObservations(const State& state,
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
