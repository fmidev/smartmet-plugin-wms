// ======================================================================
/*!
 * \brief Sampling details
 */
// ======================================================================

#pragma once

#include <json/json.h>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Projection;
class State;

class Sampling
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;
  std::optional<double> getResolution(const Projection& theProjection) const;

  std::optional<double> maxresolution;
  std::optional<double> minresolution;

  std::optional<double> resolution;
  std::optional<double> relativeresolution;

 private:
};  // class Sampling

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
