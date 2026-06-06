// ======================================================================
/*!
 * \brief WebP rendering options
 */
// ======================================================================

#pragma once

#include <giza/WebpOptions.h>
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

class Webp
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  Giza::WebpOptions options;

 private:
};  // class Webp

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
