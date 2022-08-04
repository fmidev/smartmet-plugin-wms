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

struct StationSymbolPriority
{
  int fmisid;
  double longitude;
  double latitude;
  int symbol;
  int priority;
  double x() const { return longitude; }
  double y() const { return latitude; }
};

using StationSymbolPriorities = std::vector<StationSymbolPriority>;

// Results from DB mapped by FMISID
using ResultSet = std::map<int, std::vector<TS::TimeSeries>>;

class ObservationLayer : public Layer
{
 public:
  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 protected:
  virtual StationSymbolPriorities processResultSet(
      const State& theState,
      const ResultSet& theResultSet,
      const boost::posix_time::ptime& requested_timestep) const = 0;
  virtual void getParameters(const boost::posix_time::ptime& requested_timestep,
                             std::vector<SmartMet::Spine::Parameter>& parameters,
                             boost::posix_time::ptime& starttime,
                             boost::posix_time::ptime& endtime) const = 0;

  ResultSet getObservations(State& theState,
                            const std::vector<SmartMet::Spine::Parameter>& parameters,
                            const boost::posix_time::ptime& starttime,
                            const boost::posix_time::ptime& endtime) const;
  ResultSet getObservations(State& theState,
                            const boost::posix_time::ptime& requested_timestep) const;

  std::string keyword;
  std::string fmisids;
  unsigned int mindistance{0};
  SmartMet::Spine::TaggedFMISIDList stationFMISIDs;

 private:
  StationSymbolPriorities getProcessedData(State& theState) const;

};  // class ObservationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
