// ======================================================================
/*!
 * \brief Symbols
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

class Symbols
{
 private:
  typedef std::map<std::string, std::string> SymbolMap;
  SymbolMap symbols;

 public:
  void init(const Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
