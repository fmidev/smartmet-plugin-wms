// ======================================================================
/*!
 * \brief An individual view in a product
 *
 * Characteristics:
 *
 *  - unique projection with specific bounding box [not yet specified]
 *  - may have 1..N layers
 *  - may override global producer and time
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layers.h"
#include "ParameterInfo.h"
#include "Properties.h"
#include <macgyver/DateTime.h>
#include <boost/optional.hpp>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Product;
class State;

class View : public Properties
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theViewCdt, State& theState);

  std::size_t hash_value(const State& theState) const;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  // TODO: Add projection
  // TODO: Add bounding box

  // Element specific:
  std::string qid;

  // SVG attributes (id, class, style, transform, filter etc)
  Attributes attributes;

  Layers layers;

 private:
};  // class View

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
