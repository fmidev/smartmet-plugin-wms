// ======================================================================
/*!
 * \brief Patterns
 *
 *  container
 */
// ======================================================================

#pragma once

#include <json/json.h>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Patterns
{
 private:
  typedef std::map<std::string, std::string> PatternMap;
  PatternMap patterns;

 public:
  void init(const Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
