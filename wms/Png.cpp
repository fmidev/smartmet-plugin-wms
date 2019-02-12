#include "Png.h"
#include "Hash.h"

#include <boost/numeric/conversion/cast.hpp>
#include <spine/Exception.h>
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

void Png::init(const Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Png JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "quality")
        options.quality = boost::numeric_cast<double>(json.asInt());
      else if (name == "errorfactor")
        options.errorfactor = boost::numeric_cast<double>(json.asInt());
      else if (name == "maxcolors")
        options.maxcolors = boost::numeric_cast<int>(json.asInt());
      else if (name == "truecolor")
        options.truecolor = json.asBool();
      else
        throw Spine::Exception(BCP, "Png does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    auto hash = Dali::hash_value(options.quality);
    Dali::hash_combine(hash, Dali::hash_value(options.errorfactor));
    Dali::hash_combine(hash, Dali::hash_value(options.errorfactor));
    Dali::hash_combine(hash, Dali::hash_value(options.maxcolors));
    Dali::hash_combine(hash, Dali::hash_value(options.truecolor));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
