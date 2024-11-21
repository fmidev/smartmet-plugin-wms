// ======================================================================
/*!
 * \brief Title
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Text.h"
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
class Properties;

class Title
{
 public:
  Title() = default;
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::string translate(const std::string& theLanguage,
                        const std::string& theDefaultLanguage) const;
  std::string translate(const std::optional<std::string>& theLanguage,
                        const std::string& theDefaultLanguage) const;

  std::string qid;
  int dx = 0;
  int dy = 0;
  Attributes attributes;

 private:
  Text text{"Title"};

};  // class Title

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
