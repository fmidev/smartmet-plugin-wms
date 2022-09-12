// ======================================================================
/*!
 * \brief Title
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Text.h"
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
class Properties;

class Title
{
 public:
  Title() = default;
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  const std::string& translate(const std::string& theLanguage) const;
  const std::string& translate(const boost::optional<std::string>& theLanguage) const;

  std::string qid;
  int dx = 0;
  int dy = 0;
  Attributes attributes;

 private:
  Text text;

};  // class Title

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
