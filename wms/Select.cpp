// ======================================================================

#include "Select.h"

#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Select
{
// ----------------------------------------------------------------------
/*!
 * \brief Select attributes for the given value
 */
// ----------------------------------------------------------------------

std::optional<AttributeSelection> attribute(const std::vector<AttributeSelection>& theSelection,
                                              double theValue)
{
  try
  {
    std::optional<AttributeSelection> ret;

    for (const auto& attrs : theSelection)
    {
      if (attrs.matches(theValue))
      {
        ret = attrs;
        break;
      }
    }
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Select
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
