// ======================================================================
/*!
 * \brief Text
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

class Text
{
 public:
  Text() = delete;
  explicit Text(std::string name);
  explicit Text(const char* name);
  Text(std::string name, const std::string& value);

  void init(Json::Value& theJson, const Config& theConfig);

  std::size_t hash_value(const State& theState) const;

  Attributes attributes;

  const std::string& translate(const std::string& theLanguage) const;
  const std::string& translate(const boost::optional<std::string>& theLanguage) const;

  bool empty() const { return translations.empty(); }
  std::string dump() const;

 private:
  std::string tag;
  std::map<std::string, std::string> translations;

};  // class Text

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
