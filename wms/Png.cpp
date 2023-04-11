#include "Png.h"
#include "Hash.h"

#include <boost/numeric/conversion/cast.hpp>
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

void Png::init(Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Png JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "quality")
        options.quality = json.asDouble();
      else if (name == "errorfactor")
        options.errorfactor = json.asDouble();
      else if (name == "maxcolors")
        options.maxcolors = json.asInt();
      else if (name == "truecolor")
        options.truecolor = json.asBool();
      else
        throw Fmi::Exception(BCP, "Png does not have a setting named '" + name + "'");
    }
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

std::size_t Png::hash_value(const State& /* theState */) const
{
  try
  {
    auto hash = Fmi::hash_value(options.quality);
    Fmi::hash_combine(hash, Fmi::hash_value(options.errorfactor));
    Fmi::hash_combine(hash, Fmi::hash_value(options.errorfactor));
    Fmi::hash_combine(hash, Fmi::hash_value(options.maxcolors));
    Fmi::hash_combine(hash, Fmi::hash_value(options.truecolor));
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
