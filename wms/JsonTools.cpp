#pragma once
#include <json/json.h>
#include <macgyver/Exception.h>
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

}  // namespace JsonTools
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
