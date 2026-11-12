// ======================================================================
/*!
 * \brief Isoline details
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Text.h"
#include <json/forwards.h>
#include <cstddef>
#include <optional>
#include <string>

namespace SmartMet::Plugin::Dali
{
class State;
}  // namespace SmartMet::Plugin::Dali

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class Isoline
{
 public:
  std::string getQid(const State& theState) const;
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  // Must be present:
  std::string qid;
  double value = 0.0;

  // SVG attributes (id, class, style, ...)
  Attributes attributes;

  std::optional<Text> label;

 private:
  mutable std::string generated_qid;

};  // class Isoline

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
