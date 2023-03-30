#pragma once
#include "Config.h"
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/optional.hpp>
#include <json/json.h>
#include <macgyver/Exception.h>
#include <macgyver/TimeZones.h>
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
Json::Value remove(Json::Value& theJson, const std::string& theName);

void remove_string(std::string& theValue, Json::Value& theJson, const std::string& theName);
void remove_string(boost::optional<std::string>& theValue,
                   Json::Value& theJson,
                   const std::string& theName,
                   const boost::optional<std::string>& theDefault = {});

void remove_int(int& theValue, Json::Value& theJson, const std::string& theName);
void remove_int(int& theValue, Json::Value& theJson, const std::string& theName, int theDefault);

void remove_int(boost::optional<int>& theValue,
                Json::Value& theJson,
                const std::string& theName,
                const boost::optional<int>& theDefault = {});

void remove_uint(boost::optional<uint>& theValue,
                 Json::Value& theJson,
                 const std::string& theName,
                 const boost::optional<uint>& theDefault = {});

void remove_double(double& theValue, Json::Value& theJson, const std::string& theName);
void remove_double(boost::optional<double>& theValue,
                   Json::Value& theJson,
                   const std::string& theName,
                   const boost::optional<double>& theDefault = {});

void remove_bool(bool& theValue, Json::Value& theJson, const std::string& theName);

void remove_bool(bool& theValue, Json::Value& theJson, const std::string& theName, bool theDefault);

void remove_tz(boost::local_time::time_zone_ptr& theZone,
               Json::Value& theJson,
               const std::string& theName,
               const Fmi::TimeZones& theTimeZones,
               const boost::local_time::time_zone_ptr& = {});

void remove_time(boost::optional<boost::posix_time::ptime>& theTime,
                 Json::Value& theJson,
                 const std::string& theName,
                 const boost::optional<boost::posix_time::ptime>& theDefault = {});

void remove_time(boost::optional<boost::posix_time::ptime>& theTime,
                 Json::Value& theJson,
                 const std::string& theName,
                 const boost::local_time::time_zone_ptr& theZone,
                 const boost::optional<boost::posix_time::ptime>& theDefault = {});

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
