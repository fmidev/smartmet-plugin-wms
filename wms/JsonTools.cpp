#include "JsonTools.h"
#include "CaseInsensitiveComparator.h"
#include "Time.h"
#include <json/json.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace JsonTools
{
Json::Value remove(Json::Value& theJson, const std::string& theName)
{
  Json::Value value;
  static_cast<void>(theJson.removeMember(theName, &value));
  return value;
}

void remove_string(std::string& theValue, Json::Value& theJson, const std::string& theName)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asString();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a string");
  }
}

void remove_string(boost::optional<std::string>& theValue,
                   Json::Value& theJson,
                   const std::string& theName,
                   const boost::optional<std::string>& theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asString();
    else if (theDefault)
      theValue = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a string");
  }
}

void remove_int(int& theValue, Json::Value& theJson, const std::string& theName)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asInt();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP,
                                "Failed to extract JSON field '" + theName + "' as an integer");
  }
}

void remove_int(int& theValue, Json::Value& theJson, const std::string& theName, int theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asInt();
    else
      theValue = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP,
                                "Failed to extract JSON field '" + theName + "' as an integer");
  }
}

void remove_int(boost::optional<int>& theValue,
                Json::Value& theJson,
                const std::string& theName,
                const boost::optional<int>& theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asInt();
    else if (theDefault)
      theValue = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP,
                                "Failed to extract JSON field '" + theName + "' as an integer");
  }
}

void remove_uint(uint& theValue, Json::Value& theJson, const std::string& theName)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asUInt();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(
        BCP, "Failed to extract JSON field '" + theName + "' as an unsigned integer");
  }
}

void remove_uint(boost::optional<uint>& theValue,
                 Json::Value& theJson,
                 const std::string& theName,
                 const boost::optional<uint>& theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asUInt();
    else if (theDefault)
      theValue = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(
        BCP, "Failed to extract JSON field '" + theName + "' as an unsigned integer");
  }
}

void remove_double(double& theValue, Json::Value& theJson, const std::string& theName)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asDouble();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a double");
  }
}

void remove_double(boost::optional<double>& theValue,
                   Json::Value& theJson,
                   const std::string& theName,
                   const boost::optional<double>& theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asDouble();
    else if (theDefault)
      theValue = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a double");
  }
}

void remove_bool(bool& theValue, Json::Value& theJson, const std::string& theName)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asBool();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a boolean");
  }
}

void remove_bool(bool& theValue, Json::Value& theJson, const std::string& theName, bool theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (!json.isNull())
      theValue = json.asBool();
    else
      theValue = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a boolean");
  }
}

void remove_tz(boost::local_time::time_zone_ptr& theZone,
               Json::Value& theJson,
               const std::string& theName,
               const Fmi::TimeZones& theTimeZones,
               const boost::local_time::time_zone_ptr& theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (json.isString())
      theZone = parse_timezone(json.asString(), theTimeZones);
    else if (!json.isNull())
      throw Fmi::Exception(BCP, "Failed to parse tz setting: '" + json.asString());
    else if (theDefault)
      theZone = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP,
                                "Failed to extract JSON field '" + theName + "' as a timezone");
  }
}

void remove_time(boost::optional<boost::posix_time::ptime>& theTime,
                 Json::Value& theJson,
                 const std::string& theName,
                 const boost::optional<boost::posix_time::ptime>& theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (json.isString())
    {
      WMS::CaseInsensitiveComparator cicomp;
      auto str = json.asString();
      if (cicomp(str, "current"))
        theTime = boost::posix_time::second_clock::universal_time();
      else
        theTime = parse_time(str);
    }
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      theTime = parse_time(Fmi::to_string(tmp));
    }
    else if (!json.isNull())
      throw Fmi::Exception(BCP, "Failed to parse time setting: '" + json.asString());
    else if (theDefault)
      theTime = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a time");
  }
}

void remove_time(boost::optional<boost::posix_time::ptime>& theTime,
                 Json::Value& theJson,
                 const std::string& theName,
                 const boost::local_time::time_zone_ptr& theZone,
                 const boost::optional<boost::posix_time::ptime>& theDefault)
{
  try
  {
    auto json = remove(theJson, theName);
    if (json.isString())
    {
      WMS::CaseInsensitiveComparator cicomp;
      auto str = json.asString();
      if (cicomp(str, "current"))
        theTime = boost::posix_time::second_clock::universal_time();
      else
        theTime = parse_time(str);
    }
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      theTime = parse_time(Fmi::to_string(tmp), theZone);
    }
    else if (!json.isNull())
      throw Fmi::Exception(BCP, "Failed to parse time setting: '" + json.asString());
    else if (theDefault)
      theTime = theDefault;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract JSON field '" + theName + "' as a time");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract a set of strings
 *
 * Allowed: "string" and ["string1" ... ]
 */
// ----------------------------------------------------------------------

void extract_set(const std::string& theName,
                 std::set<std::string>& theSet,
                 const Json::Value& theJson)
{
  try
  {
    if (theJson.isString())
      theSet.insert(theJson.asString());
    else if (theJson.isArray())
    {
      for (const auto& json : theJson)
      {
        if (json.isString())
          theSet.insert(json.asString());
        else
          throw Fmi::Exception(BCP, "The '" + theName + "' array must contain strings only!");
      }
    }
    else
      throw Fmi::Exception(
          BCP, "The '" + theName + "' setting must be a string or an array of strings!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract a set of integers
 *
 * Allowed: int and [int1 ... ]
 * Note that we do not allow doubles, whereas the function below for doubles allows integers.
 */
// ----------------------------------------------------------------------

void extract_set(const std::string& theName, std::set<int>& theSet, const Json::Value& theJson)
{
  try
  {
    if (theJson.isInt())
      theSet.insert(theJson.asInt());
    else if (theJson.isArray())
    {
      for (const auto& json : theJson)
      {
        if (json.isInt())
          theSet.insert(json.asInt());
        else
          throw Fmi::Exception(BCP, "The '" + theName + "' array must contain integers only");
      }
    }
    else
      throw Fmi::Exception(
          BCP, "The '" + theName + "' setting must be an integer or an array of integers");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract a set of doubles
 *
 * Allowed: int/double and [int1/double1 ... ]
 */
// ----------------------------------------------------------------------

void extract_set(const std::string& theName, std::set<double>& theSet, const Json::Value& theJson)
{
  try
  {
    if (theJson.isInt() || theJson.isDouble())
      theSet.insert(theJson.asDouble());
    else if (theJson.isArray())
    {
      for (const auto& json : theJson)
        theSet.insert(json.asDouble());
    }
    else
      throw Fmi::Exception(BCP,
                           "The '" + theName + "' setting must be a double or an array of doubles");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract a vector of doubles
 */
// ----------------------------------------------------------------------

void extract_vector(const std::string& theName,
                    std::vector<double>& theVector,
                    const Json::Value& theJson)
{
  try
  {
    if (theJson.isInt() || theJson.isDouble())
      theVector.push_back(theJson.asDouble());
    else if (theJson.isArray())
    {
      for (const auto& json : theJson)
        theVector.push_back(json.asDouble());
    }
    else
      throw Fmi::Exception(BCP,
                           "The '" + theName + "' setting must be a double or an array of doubles");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace JsonTools
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
