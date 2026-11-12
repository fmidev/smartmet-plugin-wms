#include "Stations.h"
#include "Hash.h"
#include "JsonTools.h"
#include <json/forwards.h>
#include <macgyver/Exception.h>
#include <string>

namespace SmartMet::Plugin::Dali
{
class State;
}  // namespace SmartMet::Plugin::Dali

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

void Stations::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    JsonTools::extract_array("stations", stations, theJson, theConfig);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
