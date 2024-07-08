// ======================================================================
/*!
 * \brief WindRose details
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Attributes.h"
#include "Connector.h"
#include <optional>
#include <json/json.h>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class WindRose
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  int minpercentage = 0;
  int radius = 20;  // readable size
  int sectors = 8;  // N, NE ... SW, W, NW
  std::optional<std::string> symbol;
  Attributes attributes;
  std::optional<Connector> connector;
  std::optional<std::string> parameter;
  std::vector<AttributeSelection> limits;

 private:
};  // class WindRose

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
