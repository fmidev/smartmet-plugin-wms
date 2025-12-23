#include "Animation.h"
#include "Hash.h"
#include "JsonTools.h"

#include <boost/numeric/conversion/cast.hpp>
#include <macgyver/Exception.h>
#include <stdexcept>
#include <spine/Json.h>

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

void Animation::init(Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Animation JSON is not a JSON object");


    enabled = false;
    timesteps = 1;
    timestep_interval = 0;
    data_timestep = 60; // Minutes
    loopsteps = 1;
    loopstep_interval = 0;

    JsonTools::remove_bool(enabled, theJson, "enabled");
    JsonTools::remove_int(timesteps, theJson, "timesteps");
    JsonTools::remove_int(timestep_interval, theJson, "timestep_interval");
    JsonTools::remove_int(data_timestep, theJson, "data_timestep");
    JsonTools::remove_int(loopsteps, theJson, "loopsteps");
    JsonTools::remove_int(loopstep_interval, theJson, "loopstep_interval");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t Animation::hash_value(const State& /* theState */) const
{
  try
  {
    auto hash = Fmi::hash_value(enabled);
    Fmi::hash_combine(hash, Fmi::hash_value(timesteps));
    Fmi::hash_combine(hash, Fmi::hash_value(timestep_interval));
    Fmi::hash_combine(hash, Fmi::hash_value(data_timestep));
    Fmi::hash_combine(hash, Fmi::hash_value(loopsteps));
    Fmi::hash_combine(hash, Fmi::hash_value(loopstep_interval));

    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
