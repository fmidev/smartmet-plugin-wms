// ======================================================================
/*!
 * \brief Observation layer
 */
// ======================================================================

#pragma once

#ifndef WITHOUT_OBSERVATION

#include "Layer.h"
#include <engines/observation/Settings.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

// Results from DB mapped by FMISID
using ResultSet = std::map<int, std::vector<Spine::TimeSeries::TimeSeries>>;

class ObservationLayer : public Layer
{
public:

  virtual void init(const Json::Value& theJson,
					const State& theState,
                    const Config& theConfig,
					const Properties& theProperties);
    
  virtual std::size_t hash_value(const State& theState) const;
  
  virtual ResultSet getObservations(State& theState, const std::vector<SmartMet::Spine::Parameter>& parameters, const boost::posix_time::ptime& starttime, const boost::posix_time::ptime& endtime) const;
  
protected:

  std::string keyword;
  std::string fmisids;
  SmartMet::Spine::TaggedFMISIDList stationFMISIDs;

};  // class ObservationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
