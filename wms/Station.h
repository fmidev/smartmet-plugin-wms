// ======================================================================
/*!
 * \brief Station
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Title.h"
#include <boost/optional.hpp>
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class Station
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  boost::optional<int> fmisid;
  // boost::optional<int> lpnn;
  // boost::optional<int> wmo;
  boost::optional<double> longitude;
  boost::optional<double> latitude;
  boost::optional<std::string> symbol;
  Attributes attributes;
  boost::optional<Title> title;

};  // class Station

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
