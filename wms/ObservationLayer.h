// ======================================================================
/*!
 * \brief Observation layer
 */
// ======================================================================

#pragma once

#include "Label.h"
#include "Layer.h"
#include "Positions.h"
#include <boost/optional.hpp>
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
class PointData;

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

class ObservationLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 protected:
  virtual StationSymbolPriorities processResultSet(
      const std::vector<PointData>& theResultSet) const = 0;

  virtual std::vector<std::string> getParameters() const = 0;

  // Grid coordinate settings
  boost::optional<Positions> positions;
  // Stations distance limit in kilometers
  double maxdistance = 5;

  // Label settings
  Label label;

  unsigned int mindistance = 0;
  int missing_symbol = 106;  // Use zero to disable

  // std::string fmisids;
  // SmartMet::Spine::TaggedFMISIDList stationFMISIDs;

 private:
  StationSymbolPriorities getProcessedData(State& theState) const;

};  // class ObservationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
