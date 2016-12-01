// ======================================================================
/*!
 * \brief Time related tools
 */
// ======================================================================

#pragma once
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
boost::posix_time::ptime parse_time(const std::string& theTime);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
