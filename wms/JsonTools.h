#pragma once
#include "Config.h"
#include <json/json.h>
#include <macgyver/Exception.h>
#include <macgyver/LocalDateTime.h>
#include <macgyver/TimeZones.h>
#include <optional>
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
void remove_string(std::optional<std::string>& theValue,
                   Json::Value& theJson,
                   const std::string& theName,
                   const std::optional<std::string>& theDefault = {});

void remove_int(int& theValue, Json::Value& theJson, const std::string& theName);
void remove_int(int& theValue, Json::Value& theJson, const std::string& theName, int theDefault);

void remove_int(std::optional<int>& theValue,
                Json::Value& theJson,
                const std::string& theName,
                const std::optional<int>& theDefault = {});

void remove_uint(uint& theValue, Json::Value& theJson, const std::string& theName);

void remove_uint(std::optional<uint>& theValue,
                 Json::Value& theJson,
                 const std::string& theName,
                 const std::optional<uint>& theDefault = {});

void remove_double(double& theValue, Json::Value& theJson, const std::string& theName);
void remove_double(std::optional<double>& theValue,
                   Json::Value& theJson,
                   const std::string& theName,
                   const std::optional<double>& theDefault = {});

void remove_bool(bool& theValue, Json::Value& theJson, const std::string& theName);

void remove_bool(bool& theValue, Json::Value& theJson, const std::string& theName, bool theDefault);

void remove_tz(Fmi::TimeZonePtr& theZone,
               Json::Value& theJson,
               const std::string& theName,
               const Fmi::TimeZones& theTimeZones,
               const Fmi::TimeZonePtr& theDefault = {});

void remove_time(std::optional<Fmi::DateTime>& theTime,
                 Json::Value& theJson,
                 const std::string& theName,
                 const std::optional<Fmi::DateTime>& theDefault = {});

void remove_time(std::optional<Fmi::DateTime>& theTime,
                 Json::Value& theJson,
                 const std::string& theName,
                 const Fmi::TimeZonePtr& theZone,
                 const std::optional<Fmi::DateTime>& theDefault = {});

// extract a set of strings
void extract_set(const std::string& theName,
                 std::set<std::string>& theSet,
                 const Json::Value& theJson);

// extract a set of integers
void extract_set(const std::string& theName, std::set<int>& theSet, const Json::Value& theJson);

// extract a set of doubles
void extract_set(const std::string& theName, std::set<double>& theSet, const Json::Value& theJson);

// extract a vector of doubles
void extract_vector(const std::string& theName,
                    std::vector<double>& theVector,
                    const Json::Value& theJson);

// extract a vector of integers
void extract_vector(const std::string& theName,
                    std::vector<int>& theVector,
                    const Json::Value& theJson);

// extract a vector of unsigned integers
void extract_vector(const std::string& theName,
                    std::vector<unsigned int>& theVector,
                    const Json::Value& theJson);

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
