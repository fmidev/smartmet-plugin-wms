// ======================================================================
/*!
 * \brief Settings for numeric labels
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
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
  void init(const Json::Value& theJson, const Config& theConfig);
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
  std::string format = "%0.f";
  std::string missing = "-";
  int precision = 0;
  std::string prefix = "";
  std::string suffix = "";
  // Signed prefixes
  std::string plusprefix = "";   // for numbers >= 0
  std::string minusprefix = "";  // for numbers < 0

  void setLocale(const std::string& theLocale);
  // shared because otherwise the compiler would force definitions for all constructors
  boost::shared_ptr<std::ostringstream> formatter{new std::ostringstream};

};  // class Label

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
