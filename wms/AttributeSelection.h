// ======================================================================
/*!
 * \brief Attributes which are valid only for specific values
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <boost/optional.hpp>
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class AttributeSelection
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  boost::optional<double> value;
  boost::optional<double> lolimit;
  boost::optional<double> hilimit;
  boost::optional<std::string> symbol;
  std::map<std::string, std::string> translations;
  Attributes attributes;

  bool matches(double theValue) const;
  boost::optional<std::string> remove(const std::string& theName);

 private:
};  // class AttributeSelection

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
