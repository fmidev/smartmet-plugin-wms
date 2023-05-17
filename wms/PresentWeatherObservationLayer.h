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
  std::vector<std::string> getParameters() const override;

  StationSymbolPriorities processResultSet(
      const std::vector<PointData>& theResultSet) const override;

  int get_symbol(float wawa) const;
  int get_symbol_priority(int symbol) const;

};  // class PresentWeatherObservationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
