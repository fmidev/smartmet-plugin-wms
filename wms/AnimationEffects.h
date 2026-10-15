// ======================================================================
/*!
 * \brief Animation details
 */
// ======================================================================

#pragma once

#include <json/json.h>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class AnimationEffects
{
  public:
    class Effect
    {
      public:
       Effect() {}

       bool enabled{false};
       std::string effect;
       int start_step{0};
       int end_step{10000};
    };

  void init(Json::Value& theJson);
  void initEffect(Json::Value& theJson);
  void initEffects(Json::Value& theJson);
  std::size_t hash_value() const;

  std::vector<Effect> effects;

};  // class AnimationEffect

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
