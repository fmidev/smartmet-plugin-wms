#include "Time.h"

#include <spine/Exception.h>
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

boost::posix_time::ptime parse_time(const std::string& theTime)
{
  try
  {
    auto t = Fmi::TimeParser::parse(theTime);

    if (Fmi::TimeParser::looks(theTime) != "offset")
      return t;

    return boost::posix_time::ptime(t.date(),
                                    boost::posix_time::hours(t.time_of_day().hours() + 1));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
