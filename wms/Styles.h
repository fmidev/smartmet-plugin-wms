// ======================================================================
/*!
 * \brief Styles container
 */
// ======================================================================

#pragma once

#include <json/forwards.h>
#include <cstddef>
#include <map>
#include <string>

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
  using Style = std::map<std::string, std::string>;
  using StyleMap = std::map<std::string, Style>;
  StyleMap styles;

 public:
  void init(Json::Value& theJson, const Config& theConfig);
  void generate(CTPP::CDT& theGlobals, State& theState) const;
  std::size_t hash_value(const State& theState) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
