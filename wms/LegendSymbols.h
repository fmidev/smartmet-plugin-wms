// ======================================================================
/*!
 * \brief LegendSymbol
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class LegendSymbols
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::optional<std::string> css;
  std::optional<std::string> symbol;
  std::optional<std::string> start;
  std::optional<std::string> end;
  std::optional<std::string> missing;
  Attributes attributes;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
