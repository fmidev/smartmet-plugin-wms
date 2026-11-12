// ======================================================================
/*!
 * \brief Patterns
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

class Patterns
{
 private:
  using PatternMap = std::map<std::string, std::string>;
  PatternMap patterns;

 public:
  void init(Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
