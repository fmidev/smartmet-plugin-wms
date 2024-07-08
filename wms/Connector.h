// ======================================================================
/*!
 * \brief Connector details
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <optional>
#include <json/json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Connector
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  int startoffset;
  int endoffset;
  Attributes attributes;

 private:
};  // class Connector

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
