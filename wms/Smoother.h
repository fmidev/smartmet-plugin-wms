// ======================================================================
/*!
 * \brief Smoother details
 */
// ======================================================================

#pragma once

#include <json/json.h>
#include <trax/SmoothOptions.h>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Smoother
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  // Legacy 2D Savitzky-Golay smoother.
  std::optional<double> size;
  std::optional<double> degree;

  // Opt-in Trax grid smoother (box / median / morphology). Populated by init()
  // when a "method" is given. It is an independent path from size/degree; when
  // set and active the contour engine applies it in preference to the
  // Savitzky-Golay filter. Built here so every layer just forwards it to the
  // contour engine Options.
  std::optional<Trax::SmoothOptions> trax_options;

 private:
};  // class Smoother

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
