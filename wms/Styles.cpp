#include "Styles.h"
#include "Config.h"
#include "Hash.h"
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <macgyver/StringConversion.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
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

void Styles::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto class_members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& class_name, class_members)
    {
      const Json::Value& class_json = theJson[class_name];

      if (!class_json.isObject())
        throw Spine::Exception(
            BCP,
            "Invalid object type in defs.styles initialization, expecting name-value pairs for CSS "
            "class '" +
                class_name + "'");

      const auto members = class_json.getMemberNames();
      BOOST_FOREACH (const auto& name, members)
      {
        const Json::Value json = class_json[name];

        switch (json.type())
        {
          case Json::nullValue:
            break;
          case Json::intValue:
          {
            styles[class_name][name] = Fmi::to_string(json.asInt());
            break;
          }
          case Json::uintValue:
          {
            styles[class_name][name] = Fmi::to_string(json.asUInt());
            break;
          }
          case Json::realValue:
          {
            styles[class_name][name] = Fmi::to_string(json.asDouble());
            break;
          }
          case Json::stringValue:
          case Json::booleanValue:
          {
            styles[class_name][name] = json.asString();
            break;
          }
          case Json::arrayValue:
          {
            throw Spine::Exception(BCP, "JSON arrays are not allowed in styles");
          }
          case Json::objectValue:
          {
            throw Spine::Exception(BCP, "JSON hashes are not allowed in styles");
          }
        }
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the definitions into the template hash tables
 */
// ----------------------------------------------------------------------

void Styles::generate(CTPP::CDT& theGlobals, State& theState) const
{
  try
  {
    // Add to styles
    BOOST_FOREACH (const auto& style, styles)
    {
      CTPP::CDT css(CTPP::CDT::HASH_VAL);
      BOOST_FOREACH (const auto& setting, style.second)
      {
        css[setting.first] = setting.second;
      }
      theGlobals["styles"][style.first] = css;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Styles::hash_value(const State& theState) const
{
  try
  {
    return Dali::hash_value(styles);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
