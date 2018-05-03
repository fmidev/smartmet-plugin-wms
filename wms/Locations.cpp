#include "Locations.h"
#include "Config.h"
#include "Hash.h"
#include <spine/Exception.h>
#include <spine/Json.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void Locations::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    Spine::JSON::extract_array("locations", locations, theJson, theConfig);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Locations::hash_value(const State& theState) const
{
  try
  {
    return Dali::hash_value(locations, theState);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
