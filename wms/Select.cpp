// ======================================================================

#include "Select.h"

#include <spine/Exception.h>

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

boost::optional<AttributeSelection> attribute(const std::vector<AttributeSelection> theSelection,
                                              double theValue)
{
  try
  {
    boost::optional<AttributeSelection> ret;

    for (const auto& attrs : theSelection)
    {
      if (attrs.matches(theValue))
      {
        ret.reset(attrs);
        break;
      }
    }
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Select
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
