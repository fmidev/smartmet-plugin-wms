// ======================================================================
/*!
 * \brief Observation
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Label.h"
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
