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
      else if (name == "frames")
      {
        int n = json.asInt();
        if (n < 1 || n > 1000)
          throw Fmi::Exception(BCP, "Webp 'frames' must be in the range 1...1000");
        frames = n;
      }
      else if (name == "frame_duration")
      {
        frame_duration = json.asInt();
        if (frame_duration < 1)
          throw Fmi::Exception(BCP, "Webp 'frame_duration' must be at least 1 millisecond");
      }
      else if (name == "loop")
      {
        loop = json.asInt();
        if (loop < 0)
          throw Fmi::Exception(BCP, "Webp 'loop' must be nonnegative, zero meaning forever");
      }
      else if (name == "accumulate")
        accumulate = json.asBool();
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
    auto hash = Fmi::hash_value(options.level);
    Fmi::hash_combine(hash, Fmi::hash_value(frames));
    Fmi::hash_combine(hash, Fmi::hash_value(frame_duration));
    Fmi::hash_combine(hash, Fmi::hash_value(loop));
    Fmi::hash_combine(hash, Fmi::hash_value(accumulate));
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
