// ======================================================================
/*!
 * \brief Selection utilities
 */
// ----------------------------------------------------------------------

#include "AttributeSelection.h"
#include <optional>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Select
{
std::optional<AttributeSelection> attribute(const std::vector<AttributeSelection> &theSelection,
                                            double theValue);
}  // namespace Select
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
