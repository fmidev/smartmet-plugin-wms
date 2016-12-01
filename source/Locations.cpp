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
    SmartMet::Spine::JSON::extract_array("locations", locations, theJson, theConfig);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
