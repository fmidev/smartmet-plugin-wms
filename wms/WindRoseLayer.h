// ======================================================================
/*!
 * \brief WindRose layer
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include "Observations.h"
#include "WindRose.h"
#include "WindRoseData.h"
#include "Stations.h"
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class WindRoseLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

 private:
  std::string timezone = "UTC";  // timezone for time interval
  int starttimeoffset = 0;       // offset from valid time
  int endtimeoffset = 24;        // offset from end time

  WindRose windrose;          // wind rose appearance definitions
  Observations observations;  // which observations to render
  Stations stations;          // which stations to use

  std::map<int, WindRoseData> getObservations(State& theState,
                                              const boost::posix_time::ptime& theStartTime,
                                              const boost::posix_time::ptime& theEndTime) const;

};  // class WindRoseLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
