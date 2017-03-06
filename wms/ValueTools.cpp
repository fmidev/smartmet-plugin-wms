#include "ValueTools.h"
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Extract numeric TimeSeries value
 */
// ----------------------------------------------------------------------

double get_double(const Spine::TimeSeries::Value& value)
{
  if (const double* dvalue = boost::get<double>(&value))
    return *dvalue;

  if (const int* ivalue = boost::get<int>(&value))
    return *ivalue;

  // None, std::string, LonLat and local_date_time not accepted. See spine/TimeSeries.h

  throw Spine::Exception(BCP, "Failed to convert observation engine value to a number!", NULL);
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract numeric TimeSeries value
 */
// ----------------------------------------------------------------------

double get_double(const Spine::TimeSeries::TimedValue& timedvalue)
{
  return get_double(timedvalue.value);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
