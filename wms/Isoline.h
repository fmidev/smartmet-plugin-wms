// ======================================================================
/*!
 * \brief Isoline details
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

class Isoline
{
 public:
  Isoline() : value(0) {}
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  // Must be present:
  std::string qid;
  double value;

  // SVG attributes (id, class, style, ...)
  Attributes attributes;

 private:
};  // class Isoline

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
