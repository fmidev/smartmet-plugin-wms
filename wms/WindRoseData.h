// ----------------------------------------------------------------------
/*!
 * \brief Structure for wind rose observations
 */
// ----------------------------------------------------------------------

#pragma once

#ifndef WITHOUT_OBSERVATION

#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct WindRoseData
{
  std::vector<double> percentages;
  std::vector<double> max_winds;

  double mean_wind;
  double max_wind;
  double mean_temperature;

  double longitude;
  double latitude;

  bool valid;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
