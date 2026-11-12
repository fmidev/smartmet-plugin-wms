// ======================================================================
/*!
 * \brief Heatmap
 */
// ======================================================================

#pragma once

#include <heatmap/heatmap.h>
#include <json/forwards.h>
#include <json/json.h>
#include <cstddef>
#include <memory>
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

class Heatmap
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;
  std::unique_ptr<heatmap_stamp_t, void (*)(heatmap_stamp_t*)> getStamp(unsigned radius);

  std::optional<double> resolution;
  std::optional<double> radius;
  std::optional<std::string> kernel;
  std::optional<double> deviation;

  unsigned max_points = 0;                          // Configured or default max heatmap size
  const unsigned max_max_points = 2000 * 2000;  // Max allowed heatmap size

 private:
};  // class Heatmap

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
