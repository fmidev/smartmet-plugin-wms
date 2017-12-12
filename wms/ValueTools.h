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

int get_fmisid(const Spine::TimeSeries::Value& value);
int get_fmisid(const Spine::TimeSeries::TimedValue& value);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
