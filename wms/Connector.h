// ======================================================================
/*!
 * \brief Connector details
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <json/forwards.h>
#include <cstddef>

namespace SmartMet::Plugin::Dali
{
class Config;
class State;
}  // namespace SmartMet::Plugin::Dali

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

  int startoffset = 0;
  int endoffset = 0;
  Attributes attributes;

 private:
};  // class Connector

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
