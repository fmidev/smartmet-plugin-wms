#include "Time.h"

#include <macgyver/TimeParser.h>
#include <spine/Exception.h>

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
