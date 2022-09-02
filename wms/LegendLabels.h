// ======================================================================
/*!
 * \brief LegendLabel
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <boost/optional.hpp>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class LegendLabels
{
 public:
  LegendLabels();
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::string type;
  boost::optional<std::string> format;
  int dx = 0;
  int dy = 0;
  std::string separator;
  std::map<std::string, std::string> conversions;

  Attributes attributes;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
