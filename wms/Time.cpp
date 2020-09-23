#include "Time.h"

#include <macgyver/TimeParser.h>
#include <macgyver/Exception.h>

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

boost::posix_time::ptime parse_time(const std::string& theTime)
{
  try
  {
    auto t = Fmi::TimeParser::parse(theTime);

    if (Fmi::TimeParser::looks(theTime) != "offset")
      return t;

    return {t.date(), boost::posix_time::hours(t.time_of_day().hours() + 1)};
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

boost::posix_time::ptime parse_time(const std::string& theTime,
                                    const boost::local_time::time_zone_ptr& theZone)
{
  try
  {
    if (!theZone)
      return parse_time(theTime);

    auto t = Fmi::TimeParser::parse(theTime, theZone).utc_time();

    if (Fmi::TimeParser::looks(theTime) != "offset")
      return t;

    return {t.date(), boost::posix_time::hours(t.time_of_day().hours() + 1)};
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

boost::local_time::time_zone_ptr parse_timezone(const std::string& theName,
                                                const Fmi::TimeZones& theZones)
{
  return theZones.time_zone_from_string(theName);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
