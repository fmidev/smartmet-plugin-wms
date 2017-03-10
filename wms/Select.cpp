// ======================================================================

#include "Select.h"
#include <boost/foreach.hpp>
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

    BOOST_FOREACH (const auto& attrs, theSelection)
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Select
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
