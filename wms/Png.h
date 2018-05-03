// ======================================================================
/*!
 * \brief PNG rendering options
 */
// ======================================================================

#pragma once

#include <giza/ColorMapOptions.h>
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Png
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  Giza::ColorMapOptions options;

 private:
};  // class Png

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
