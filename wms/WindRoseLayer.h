// ======================================================================
/*!
 * \brief WindRose layer
 */
// ======================================================================

#pragma once

#ifndef WITHOUT_OBSERVATION

#include "Layer.h"
#include "Observations.h"
#include "Stations.h"
#include "WindRose.h"
#include "WindRoseData.h"
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
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  std::string timezone = "UTC";  // timezone for time interval
  int starttimeoffset = 0;       // offset from valid time
  int endtimeoffset = 24;        // offset from end time

  WindRose windrose;          // wind rose appearance definitions
  Observations observations;  // which observations to render
  Stations stations;          // which stations to use

  std::map<int, WindRoseData> getObservations(State& theState,
                                              const Fmi::DateTime& theStartTime,
                                              const Fmi::DateTime& theEndTime) const;

};  // class WindRoseLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
