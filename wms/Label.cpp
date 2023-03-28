#include "Label.h"
#include "Config.h"
#include "Hash.h"

#include <boost/format.hpp>
#include <boost/locale.hpp>
#include <macgyver/Exception.h>
#include <cfenv>
#include <cmath>
#include <iomanip>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// This is thread safe
void set_rounding(const std::string& rounding)
{
  if (rounding == "tonearest")
    std::fesetround(FE_TONEAREST);
  else if (rounding == "towardzero")
    std::fesetround(FE_TOWARDZERO);
  else if (rounding == "upward")
    std::fesetround(FE_UPWARD);
  else if (rounding == "downward")
    std::fesetround(FE_DOWNWARD);
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Set the locale
 *
 * Generating a locale is slow, hence locale and formatter are data members
 */
// ----------------------------------------------------------------------

void Label::setLocale(const std::string& theLocale)
{
  try
  {
    if (!theLocale.empty())
    {
      boost::locale::generator gen;
      formatter->imbue(gen(theLocale));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format the value
 */
// ----------------------------------------------------------------------

std::string Label::print(double theValue) const
{
  try
  {
    // TODO(mheiskan): querydata should return NaN for kFloatMissing

    if (theValue == 32700 || !std::isfinite(theValue))
      return missing;

    double value = multiplier * theValue + offset;

    auto old_rounding = std::fegetround();

    try
    {
      set_rounding(rounding);

      if (multiple > 0)
        value = multiple * std::rint(value / multiple);

      formatter->str("");

      if (plusprefix.empty() && minusprefix.empty())
      {
        // Standard case has no sign manipulations
        *formatter << std::fixed << std::setprecision(precision) << value;
        std::fesetround(old_rounding);

        std::string ret = formatter->str();
        if (ret == "-0")
          ret = "0";

        ret = (prefix + ret + suffix);
        if (ret.length() < padding_length && padding_char != 0)
        {
          auto repeat = (padding_length - ret.length());
          ret.insert(0, repeat, padding_char);
        }
        return ret;
        //        return prefix + ret + suffix;
      }

      // Handle special sign selections
      *formatter << std::fixed << std::setprecision(precision) << std::abs(value);
      std::fesetround(old_rounding);

      std::string ret = formatter->str();
      if (value < 0)
        return prefix + minusprefix + ret + suffix;
      return prefix + plusprefix + ret + suffix;
    }
    catch (...)
    {
      std::fesetround(old_rounding);
      throw Fmi::Exception::Trace(BCP, "Operation failed!");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void Label::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "dx")
        dx = json.asInt();
      else if (name == "dy")
        dy = json.asInt();
      else if (name == "unit_conversion")
        unit_conversion = json.asString();
      else if (name == "multiplier")
        multiplier = json.asDouble();
      else if (name == "offset")
        offset = json.asDouble();
      else if (name == "format")
      {
        std::cerr << "Warning: label.format has been deprecated, use label.precision instead"
                  << std::endl;
      }
      else if (name == "missing")
        missing = json.asString();
      else if (name == "locale")
        this->setLocale(json.asString());
      else if (name == "precision")
        precision = json.asInt();
      else if (name == "multiple")
        multiple = json.asDouble();
      else if (name == "rounding")
      {
        rounding = json.asString();
        if (rounding != "tonearest" && rounding != "upward" && rounding != "downward" &&
            rounding != "towardzero")
          throw Fmi::Exception(BCP, "Unknown label rounding mode '" + rounding + "'");
      }
      else if (name == "prefix")
        prefix = json.asString();
      else if (name == "suffix")
        suffix = json.asString();
      else if (name == "plusprefix")
        plusprefix = json.asString();
      else if (name == "minusprefix")
        minusprefix = json.asString();
      else if (name == "orientation")
      {
        orientation = json.asString();
        if (orientation != "horizontal" && orientation != "vertical" && orientation != "auto" &&
            orientation != "gradient")
          throw Fmi::Exception(BCP, "Unknown label orientation '" + orientation + "'");
      }
      else if (name == "padding_char")
      {
        auto padding_str = json.asString();
        if (!padding_str.empty())
          padding_char = padding_str.front();
      }
      else if (name == "padding_length")
      {
        padding_length = json.asInt();
      }
      else
        throw Fmi::Exception(BCP, "Unknown setting '" + name + "'!");
    }

    if (!unit_conversion.empty())
    {
      auto conv = theConfig.unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }
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

std::size_t Label::hash_value(const State& /* theState */) const
{
  try
  {
    auto hash = Fmi::hash_value(dx);
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Fmi::hash_value(orientation));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Fmi::hash_value(missing));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Fmi::hash_value(multiple));
    Fmi::hash_combine(hash, Fmi::hash_value(rounding));
    Fmi::hash_combine(hash, Fmi::hash_value(prefix));
    Fmi::hash_combine(hash, Fmi::hash_value(suffix));
    Fmi::hash_combine(hash, Fmi::hash_value(plusprefix));
    Fmi::hash_combine(hash, Fmi::hash_value(minusprefix));
    Fmi::hash_combine(hash, Fmi::hash_value(padding_char));
    Fmi::hash_combine(hash, Fmi::hash_value(padding_length));
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
