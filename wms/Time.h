// ======================================================================
/*!
 * \brief Time related tools
 */
// ======================================================================

#pragma once
#include <macgyver/DateTime.h>
#include <macgyver/LocalDateTime.h>
#include <macgyver/TimeZones.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
Fmi::DateTime parse_time(const std::string& theTime);

Fmi::DateTime parse_time(const std::string& theTime, const Fmi::TimeZonePtr& theZone);

Fmi::TimeZonePtr parse_timezone(const std::string& theName, const Fmi::TimeZones& theZones);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
