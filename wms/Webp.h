// ======================================================================
/*!
 * \brief WebP rendering options
 */
// ======================================================================

#pragma once

#include <giza/WebpOptions.h>
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
class State;

class Webp
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  // Lossless preset level 1 is the highest one that avoids libwebp's costly
  // BackwardReferencesTraceBackwards search (triggered at quality >= 25).
  Giza::WebpOptions options{1};

  // Animation settings. Setting 'frames' enables animated WebP output: layers
  // supporting time animation (flash observations) bucket their symbols by
  // observation time into this many frames over the layer time interval.
  std::optional<int> frames;
  int frame_duration = 100;  // milliseconds per frame
  int loop = 0;              // animation loop count, 0 = loop forever
  bool accumulate = false;   // true = symbols accumulate over the loop instead
                             // of each frame showing only its own time bucket

 private:
};  // class Webp

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
