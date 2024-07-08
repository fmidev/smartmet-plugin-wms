// ======================================================================
/*!
 * \brief Settings for numeric labels
 */
// ======================================================================

#pragma once

#include <optional>
#include <memory>
#include <json/json.h>
#include <sstream>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Label
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;
  std::string print(double theValue) const;

  // Offset from actual image position
  int dx = 0;
  int dy = 0;

  std::string orientation = "horizontal";  // horizontal, auto

 private:
  // Value conversion
  std::string unit_conversion;
  double multiplier = 1.0;
  double offset = 0.0;

  // Printing
  std::string missing = "-";
  int precision = 0;

  double multiple = 0;                 // used if non-zero
  std::string rounding = "tonearest";  // tonearest|upward|downward|towardzero

  std::string prefix;
  std::string suffix;
  // Signed prefixes
  std::string plusprefix;   // for numbers >= 0
  std::string minusprefix;  // for numbers < 0
  // Padding char, space is default padding char
  char padding_char = ' ';
  // If label is shorter than padding_length, increase size by using padding char
  size_t padding_length = 0;

  void setLocale(const std::string& theLocale);
  // shared because otherwise the compiler would force definitions for all constructors
  std::shared_ptr<std::ostringstream> formatter{new std::ostringstream};

};  // class Label

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
