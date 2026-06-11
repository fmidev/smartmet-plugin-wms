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

  // Lossless preset level 1 is the highest one that avoids libwebp's costly
  // BackwardReferencesTraceBackwards search (triggered at quality >= 25).
  Giza::WebpOptions options{1};

 private:
};  // class Webp

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
