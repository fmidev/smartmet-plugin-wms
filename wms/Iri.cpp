#include "Iri.h"
#include <boost/algorithm/string/replace.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Iri
{
// ----------------------------------------------------------------------
/*!
 * \brief Normalize an IRI
 *
 * Note: The Dali user may specify subpaths for symbols etc.
 * To avoid name collisitions, we form the iri from the path
 * by converting path separator character '/' to an underscore.
 */
// ----------------------------------------------------------------------

std::string normalize(const std::string& thePath)
{
  // If it starts with data:, it is embedded data
  if (thePath.substr(0, 5) == "data:")
    return thePath;
  // Otherwise / should be replaced
  return boost::algorithm::replace_all_copy(thePath, "/", "_");
}

}  // namespace Iri
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
