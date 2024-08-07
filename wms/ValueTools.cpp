#include "ValueTools.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiGlobals.h>

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

double get_double(const TS::Value& value)
{
  if (const double* dvalue = std::get_if<double>(&value))
    return *dvalue;

  if (const int* ivalue = std::get_if<int>(&value))
    return *ivalue;

  return kFloatMissing;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract numeric TimeSeries value
 */
// ----------------------------------------------------------------------

double get_double(const TS::TimedValue& timedvalue)
{
  return get_double(timedvalue.value);
}

// ----------------------------------------------------------------------
/*!
 * \brief Get fmisid from TimeSeries
 *
 * Note: This is a temporary fix, since currently obsengine may return
 * fmisid as double or as string, while the proper type would be int.
 * This function should be rewritten to accept only int once the
 * other components have been fixed.
 */
// ----------------------------------------------------------------------

int get_fmisid(const TS::Value& value)
{
  if (const double* dvalue = std::get_if<double>(&value))
    return static_cast<int>(*dvalue);

  if (const int* ivalue = std::get_if<int>(&value))
    return *ivalue;

  if (const std::string* svalue = std::get_if<std::string>(&value))
    return Fmi::stoi(*svalue);

  // None, LonLat and Fmi::LocalDateTime not accepted. See spine/TimeSeries.h

  throw Fmi::Exception::Trace(BCP, "Failed to convert observation engine value to fmisid!");
}

int get_fmisid(const TS::TimedValue& timedvalue)
{
  return get_fmisid(timedvalue.value);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
