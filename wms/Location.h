// ======================================================================
/*!
 * \brief Location
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <json/json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Location
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  // See also Station.h to be consistent if you're planning to expand this class
  boost::optional<double> longitude;
  boost::optional<double> latitude;

};  // class Location

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
