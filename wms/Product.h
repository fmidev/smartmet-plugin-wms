// ======================================================================
/*!
 * \brief Full product specification
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Defs.h"
#include "ParameterInfo.h"
#include "Png.h"
#include "Projection.h"
#include "Properties.h"
#include "Text.h"
#include "Views.h"
#include "Animation.h"
#include <json/json.h>
#include <macgyver/DateTime.h>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Product : public Properties
{
 public:
  void init(Json::Value& theJson, const State& theState, const Config& theConfig);
  void check_errors(const std::string& name, std::set<std::string>& warnedURLs) const;

  void generate(CTPP::CDT& theGlobals, State& theState);
  std::size_t hash_value(const State& theState) const;

  ParameterInfos getGridParameterInfo(const State& theState) const;

  // Element specific:
  std::optional<std::string> svg_tmpl;
  std::string type;
  std::optional<int> width;
  std::optional<int> height;
  std::optional<Text> title;

  // Animation settings
  Animation animation;

  // Defs section
  Defs defs;

  // SVG attributes (id, class, style, ...)
  Attributes attributes;

  // Views
  Views views;

  // PNG rendering options
  Png png;

 private:
};  // class Product

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
