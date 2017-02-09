// ======================================================================
/*!
 * \brief Styles container
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

class Styles
{
 private:
  typedef std::map<std::string, std::string> Style;
  typedef std::map<std::string, Style> StyleMap;
  StyleMap styles;

 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  void generate(CTPP::CDT& theGlobals, State& theState) const;
  std::size_t hash_value(const State& theState) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
