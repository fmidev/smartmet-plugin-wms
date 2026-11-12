// ======================================================================
/*!
 * \brief Gradients
 *
 *  container
 */
// ======================================================================

#pragma once

#include <json/forwards.h>
#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class State;

class Gradients
{
 private:
  using GradientMap = std::map<std::string, std::string>;
  GradientMap gradients;

 public:
  void init(Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
