// ======================================================================
/*!
 * \brief Text
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <json/json.h>
#include <optional>
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

  std::string translate(const std::string& theLanguage,
                        const std::string& theDefaultLanguage) const;
  std::string translate(const std::optional<std::string>& theLanguage,
                        const std::string& theDefaultLanguage) const;

  bool empty() const { return translations.empty(); }
  std::string dump() const;

 private:
  std::string tag;
  std::map<std::string, std::string> translations;

};  // class Text

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
