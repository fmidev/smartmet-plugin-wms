// ======================================================================
/*!
 * \brief A WKT layer
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include <cstddef>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class WKTLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

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
