// ======================================================================
/*!
 * \brief Symbols
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

class Symbols
{
 private:
  using SymbolMap = std::map<std::string, std::string>;
  SymbolMap symbols;

 public:
  void init(Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
