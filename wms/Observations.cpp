#ifndef WITHOUT_OBSERVATION

#include "Observations.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
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

void Observations::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    JsonTools::extract_array("observations", observations, theJson, theConfig);
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

std::size_t Observations::hash_value(const State& theState) const
{
  try
  {
    return Dali::hash_value(observations, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
