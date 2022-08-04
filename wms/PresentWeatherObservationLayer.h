// ======================================================================
/*!
 * \brief PresentWeatherObservation layer
 */
// ======================================================================

#pragma once

#ifndef WITHOUT_OBSERVATION

#include "ObservationLayer.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class PresentWeatherObservationLayer : public ObservationLayer
{
 private:
  virtual void getParameters(const boost::posix_time::ptime& requested_timestep,
                             std::vector<SmartMet::Spine::Parameter>& parameters,
                             boost::posix_time::ptime& starttime,
                             boost::posix_time::ptime& endtime) const;
  virtual StationSymbolPriorities processResultSet(
      const State& theState,
      const ResultSet& theResultSet,
      const boost::posix_time::ptime& requested_timestep) const;
};  // class PresentWeatherObservationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
