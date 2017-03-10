#include "Stations.h"
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

void Stations::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    Spine::JSON::extract_array("stations", stations, theJson, theConfig);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Stations::hash_value(const State& theState) const
{
  try
  {
    return Dali::hash_value(stations, theState);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
