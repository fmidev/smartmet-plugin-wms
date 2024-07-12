// ======================================================================
/*!
 * \brief A WKT layer
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "Text.h"
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

class WKTLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  std::optional<std::string> tag;

 private:
  std::string wkt;
  std::optional<double> resolution;
  std::optional<double> relativeresolution;
  double precision = 1.0;

};  // class WKTLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
