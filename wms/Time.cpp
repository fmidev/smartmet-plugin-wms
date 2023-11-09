#include "Time.h"

#include <macgyver/Exception.h>
#include <macgyver/TimeParser.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Parse a time option
 *
 * Note: Relative times are rounded to the next hour
 */
// ----------------------------------------------------------------------

Fmi::DateTime parse_time(const std::string& theTime)
{
  try
  {
    auto t = Fmi::TimeParser::parse(theTime);

    if (Fmi::TimeParser::looks(theTime) != "offset")
      return t;

    return {t.date(), Fmi::Hours(t.time_of_day().hours() + 1)};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a time option with an optional time zone (default = UTC)
 */
// ----------------------------------------------------------------------

Fmi::DateTime parse_time(const std::string& theTime,
                                    const Fmi::TimeZonePtr& theZone)
{
  try
  {
    if (!theZone)
      return parse_time(theTime);

    auto t = Fmi::TimeParser::parse(theTime, theZone).utc_time();

    if (Fmi::TimeParser::looks(theTime) != "offset")
      return t;

    return {t.date(), Fmi::Hours(t.time_of_day().hours() + 1)};
  }

  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a time zone option
 */
// ----------------------------------------------------------------------

Fmi::TimeZonePtr parse_timezone(const std::string& theName,
                                                const Fmi::TimeZones& theZones)
{
  return theZones.time_zone_from_string(theName);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
