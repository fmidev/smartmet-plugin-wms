// ======================================================================
/*!
 * \brief Observation
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Label.h"
#include <json/forwards.h>
#include <cstddef>
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

class Observation
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::string parameter;
  Label label;
  Attributes attributes;

 private:
};  // class Observation

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
