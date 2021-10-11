// ======================================================================
/*!
 * \brief Observation layer
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

using StationSymbolPriorities =  std::vector<StationSymbolPriority>;

class FinnishRoadObservationLayer : public ObservationLayer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

 private:

  StationSymbolPriorities getProcessedData(State& theState) const;
  ResultSet getObservations(State& theState, const boost::posix_time::ptime& requested_timestep) const;
  StationSymbolPriorities processResultSet(State& theState, const ResultSet& theResultSet, const boost::posix_time::ptime& requested_timestep) const;

  unsigned int mindistance{0};

};  // class FinnishRoadObservationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
