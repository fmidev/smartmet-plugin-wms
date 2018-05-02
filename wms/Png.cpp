#include "Png.h"
#include "Hash.h"
#include <boost/foreach.hpp>
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

void Png::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Png JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (name == "quality")
        options.quality = boost::numeric_cast<double>(json.asInt());
      else if (name == "errorfactor")
        options.errorfactor = boost::numeric_cast<double>(json.asInt());
      else if (name == "maxcolors")
        options.maxcolors = boost::numeric_cast<double>(json.asInt());
      else if (name == "truecolor")
        options.truecolor = json.asBool();
      else
        throw Spine::Exception(BCP, "Png does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t Png::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(options.quality);
    boost::hash_combine(hash, Dali::hash_value(options.errorfactor));
    boost::hash_combine(hash, Dali::hash_value(options.errorfactor));
    boost::hash_combine(hash, Dali::hash_value(options.maxcolors));
    boost::hash_combine(hash, Dali::hash_value(options.truecolor));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
