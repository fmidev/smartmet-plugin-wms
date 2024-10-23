// ======================================================================
/*!
 * \brief Station
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Title.h"
#include <json/json.h>
#include <optional>
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
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::optional<int> fmisid;
  std::optional<int> lpnn;
  std::optional<int> wmo;
  std::optional<int> geoid;
  std::optional<double> longitude;
  std::optional<double> latitude;
  std::optional<std::string> symbol;
  Attributes attributes;
  std::optional<Title> title;

  std::optional<int> dx;
  std::optional<int> dy;

};  // class Station

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
