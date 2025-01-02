#include "Circle.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
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

void Circle::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Circle JSON must be an object");

    JsonTools::remove_double(radius, theJson, "radius");

    auto json = JsonTools::remove(theJson, "attributes");
    attributes.init(json, theConfig);

    if (radius <= 0 || radius >= 6378)
      throw Fmi::Exception(BCP, "Circle radius setting must be nonnegative and less than 6378 km");
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

std::size_t Circle::hash_value(const State& theState) const
{
  try
  {
    auto hash = Fmi::hash_value(radius);
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
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
