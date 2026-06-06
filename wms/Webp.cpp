#include "Webp.h"
#include "Hash.h"

#include <macgyver/Exception.h>

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

void Webp::init(Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Webp JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "level")
      {
        int level = json.asInt();
        if (level < 0 || level > 9)
          throw Fmi::Exception(BCP, "Webp 'level' must be in the range 0...9");
        options.level = level;
      }
      else
        throw Fmi::Exception(BCP, "Webp does not have a setting named '" + name + "'");
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

std::size_t Webp::hash_value(const State& /* theState */) const
{
  try
  {
    return Fmi::hash_value(options.level);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
