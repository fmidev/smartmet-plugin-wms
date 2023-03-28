// ======================================================================
/*!
 * \brief LegendSymbol
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

class LegendSymbols
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  boost::optional<std::string> css;
  boost::optional<std::string> symbol;
  boost::optional<std::string> start;
  boost::optional<std::string> end;
  boost::optional<std::string> missing;
  Attributes attributes;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
