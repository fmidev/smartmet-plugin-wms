// ======================================================================
/*!
 * \brief A Circle layer, typically used to draw circles around airports or radars
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Circle.h"
#include "CircleLabels.h"
#include "Layer.h"
#include <optional>
#include <string>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class CircleLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  std::string keyword;           // locations from database
  std::set<std::string> places;  // locations from database
  std::string features;          // optional comma separated list of feature codes
  bool lines = true;             // render as polylines or as polygons
  std::vector<Circle> circles;   // actual circle definitions
  CircleLabels labels;           // optional radius labels

};  // class CircleLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
