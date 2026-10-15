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

  double mean_wind = 0.0;
  double max_wind = 0.0;
  double mean_temperature = 0.0;

  double longitude = 0.0;
  double latitude = 0.0;

  bool valid = false;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
