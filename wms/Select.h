// ======================================================================
/*!
 * \brief Selection utilities
 */
// ----------------------------------------------------------------------

#include "AttributeSelection.h"
#include <boost/optional.hpp>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Select
{
boost::optional<AttributeSelection> attribute(const std::vector<AttributeSelection> &theSelection,
                                              double theValue);
}  // namespace Select
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
