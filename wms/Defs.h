// ======================================================================
/*!
 * \brief SVG header section details
 */
// ======================================================================

#pragma once

#include "CSSs.h"
#include "Filters.h"
#include "Gradients.h"
#include "Layers.h"
#include "Markers.h"
#include "Patterns.h"
#include "Properties.h"
#include "Styles.h"
#include "Symbols.h"
#include <map>
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

class Defs : public Properties
{
 public:
  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theParent);

  void generate(CTPP::CDT& theGlobals, State& theState);
  std::size_t hash_value(const State& theState) const;

  std::string qid;
  CSSs csss;
  Styles styles;
  Layers layers;
  Symbols symbols;
  Filters filters;
  Markers markers;
  Patterns patterns;
  Gradients gradients;

 private:
};  // class Defs

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
