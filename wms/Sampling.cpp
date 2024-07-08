#include "Sampling.h"
#include "Config.h"
#include "Hash.h"
#include "Projection.h"

#include <macgyver/Exception.h>
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

void Sampling::init(Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Sampling JSON is not map");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "maxresolution")
        maxresolution = json.asDouble();
      else if (name == "minresolution")
        minresolution = json.asDouble();
      else if (name == "resolution")
        resolution = json.asDouble();
      else if (name == "relativeresolution")
        relativeresolution = json.asDouble();
      else
        throw Fmi::Exception(BCP, "Sampling does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the desired resolution
 */
// ----------------------------------------------------------------------

std::optional<double> Sampling::getResolution(const Projection& theProjection) const
{
  try
  {
    // Nothing requested --> no resolution
    if (!resolution && !relativeresolution)
      return {};

    // No resolution provided? Error

    if (!theProjection.resolution)
      throw Fmi::Exception(
          BCP, "Cannot request sampling of data without a projection with a resolution available");

    // Not in valid range --> no resolution

    if (minresolution && *minresolution <= *theProjection.resolution)
      return {};

    if (maxresolution && *maxresolution > *theProjection.resolution)
      return {};

    if (resolution)
    {
      // Allow only nonnegative resolutions. In particular we want to be able
      // to disable sampling by changing the resolution to zero in a querystring.
      if (*resolution > 0)
        return resolution;
      return {};
    }

    auto scaled_resolution = (*theProjection.resolution) * (*relativeresolution);
    if (scaled_resolution > 0)
      return scaled_resolution;
    return {};
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

std::size_t Sampling::hash_value(const State& /* theState */) const
{
  try
  {
    std::size_t hash = 0;

    Fmi::hash_combine(hash, Fmi::hash_value(maxresolution));
    Fmi::hash_combine(hash, Fmi::hash_value(minresolution));
    Fmi::hash_combine(hash, Fmi::hash_value(resolution));
    Fmi::hash_combine(hash, Fmi::hash_value(relativeresolution));
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
