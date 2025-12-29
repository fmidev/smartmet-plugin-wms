// ======================================================================
/*!
 * \brief Animation details
 */
// ======================================================================

#pragma once

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

class Animation
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  bool enabled = false;           // Enables/disables the animation
  int  timesteps = 1;             // Number of animation timesteps
  int  timestep_interval = 50;    // Frame interval time (= milliseconds) between timesteps
  int  data_timestep = 60;        // Timestep length (= minutes)in in the actual data

  int  loopsteps = 1;             // Number of animation steps inside the same timestep
  int  loopstep_interval = 50;    // Frame interval time (=milliseconds) between loopsteps
};  // class Animation

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
