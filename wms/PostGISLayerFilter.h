// ======================================================================
/*!
 * \brief PostGISLayerFilter details
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <json/json.h>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class PostGISLayerFilter
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::optional<std::string> where;
  // SVG attributes for geometry (id, class, style, ...)
  Attributes attributes;
  // SVG attributes for text
  Attributes text_attributes;

 private:
};  // class PostGISLayerFilter

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
