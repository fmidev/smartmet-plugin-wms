// ======================================================================
/*!
 * \brief Utilities for handling TimeSeries values
 */
// ======================================================================

#pragma once

#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
double get_double(const TS::Value& value);
double get_double(const TS::TimedValue& timedvalue);

int get_fmisid(const TS::Value& value);
int get_fmisid(const TS::TimedValue& value);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
