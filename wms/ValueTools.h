// ======================================================================
/*!
 * \brief Utilities for handling TimeSeries values
 */
// ======================================================================

#pragma once

#include <spine/TimeSeries.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
double get_double(const Spine::TimeSeries::Value& value);
double get_double(const Spine::TimeSeries::TimedValue& timedvalue);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
