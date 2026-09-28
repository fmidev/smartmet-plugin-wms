// ======================================================================
/*!
 * \brief GetCapabilities namespace filtering
 */
// ======================================================================

#pragma once

#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
// Apply namespace filtering as in GeoServer, with a "/regex/" extension.
// The pattern comes from the request, hence regex patterns are compiled only
// once per thread and pattern. The caller limits the pattern length.
bool match_namespace_pattern(const std::string& name, const std::string& pattern);

// True if the namespace is a "/regex/" pattern
bool is_namespace_pattern(const std::string& pattern);

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
