// ======================================================================
/*!
 * \brief Observation
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Label.h"
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

class Observation
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::string parameter;
  Label label;
  Attributes attributes;

 private:
};  // class Observation

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
