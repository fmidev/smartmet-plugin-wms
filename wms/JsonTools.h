#pragma once
#include "Config.h"
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
// extract a set of strings
void extract_set(const std::string& theName,
                 std::set<std::string>& theSet,
                 const Json::Value& theJson);

// extract a set of integers
void extract_set(const std::string& theName, std::set<int>& theSet, const Json::Value& theJson);

// extract an array
template <typename Container>
void extract_array(const std::string& theName,
                   Container& theContainer,
                   Json::Value& theJson,
                   const Config& theConfig)
{
  try
  {
    if (!theJson.isArray())
      throw Fmi::Exception(BCP, theName + " setting must be an array");

    for (auto& json : theJson)
    {
      typename Container::value_type value;
      value.init(json, theConfig);
      theContainer.push_back(value);
    }
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
