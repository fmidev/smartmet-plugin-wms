// ======================================================================
/*!
 * \brief Text
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <json/json.h>
#include <boost/optional.hpp>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class Text
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  Attributes attributes;

  const std::string& translate(const std::string& theLanguage) const;
  const std::string& translate(const boost::optional<std::string>& theLanguage) const;

 private:
  std::map<std::string, std::string> translations;

};  // class Text

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
