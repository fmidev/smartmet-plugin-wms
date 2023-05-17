// ======================================================================
/*!
 * \brief FinnishRoadObservation layer
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

class FinnishRoadObservationLayer : public ObservationLayer
{
 private:
  std::vector<std::string> getParameters() const override;

  StationSymbolPriorities processResultSet(
      const std::vector<PointData>& theResultSet) const override;

  int get_symbol(int r, int rform) const;
  int get_symbol_priority(int symbol, double t2m) const;

};  // class FinnishRoadObservationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
