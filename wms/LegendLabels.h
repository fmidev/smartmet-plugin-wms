// ======================================================================
/*!
 * \brief LegendLabel
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

class LegendLabels
{
 public:
  LegendLabels() = default;
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::string type = "range";
  std::optional<std::string> format;
  int dx = 30;
  int dy = 0;
  std::string separator = (const char*)u8"\u2013";  // endash
  std::map<std::string, std::string> conversions;

  Attributes attributes;

  // Format a numeric value as a string (respecting format setting) and look up in conversions.
  // Returns the converted text, or empty string if no conversion is found.
  std::string findConversion(double theValue,
                             const std::optional<std::string>& theLanguage) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
