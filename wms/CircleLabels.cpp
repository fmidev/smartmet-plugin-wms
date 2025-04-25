
#include "CircleLabels.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{
const std::set<std::string> valid_layout_parts = {
    "north", "east", "west", "south", "top", "left", "right", "bottom"};
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void CircleLabels::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "CircleLabels JSON must be an object");

    auto json = JsonTools::remove(theJson, "layout");
    if (!json.isNull())
      JsonTools::extract_set("layout", layout, json);

    JsonTools::remove_string(prefix, theJson, "prefix");
    JsonTools::remove_string(suffix, theJson, "suffix");
    JsonTools::remove_int(dx, theJson, "dx");
    JsonTools::remove_int(dy, theJson, "dy");

    json = JsonTools::remove(theJson, "attributes");
    attributes.init(json, theConfig);

    json = JsonTools::remove(theJson, "textattributes");
    textattributes.init(json, theConfig);

    // Validation

    for (const auto& direction : layout)
    {
      if (valid_layout_parts.find(direction) == valid_layout_parts.end())
        throw Fmi::Exception(BCP, "Invalid CircleLabels layout setting")
            .addParameter("Setting", direction);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format the radius for display
 */
// ----------------------------------------------------------------------

std::string CircleLabels::format(double value) const
{
  std::string ret = prefix;
  ret += Fmi::to_string(value);
  ret += suffix;
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t CircleLabels::hash_value(const State& theState) const
{
  try
  {
    auto hash = Fmi::hash_value(layout);
    Fmi::hash_combine(hash, Fmi::hash_value(prefix));
    Fmi::hash_combine(hash, Fmi::hash_value(suffix));
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(textattributes, theState));
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
