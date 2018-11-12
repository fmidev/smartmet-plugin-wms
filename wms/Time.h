// ======================================================================
/*!
 * \brief Time related tools
 */
// ======================================================================

#pragma once
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/TimeZones.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
boost::posix_time::ptime parse_time(const std::string& theTime);

boost::posix_time::ptime parse_time(const std::string& theTime,
                                    const boost::local_time::time_zone_ptr& theZone);

boost::local_time::time_zone_ptr parse_timezone(const std::string& theName,
                                                const Fmi::TimeZones& theZones);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
