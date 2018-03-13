// ======================================================================
/*!
 * \brief Heatmap
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <json/json.h>
#include <memory>
#include <string>

#include <heatmap/heatmap.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Properties;
class State;

class Heatmap
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;
  std::unique_ptr<heatmap_stamp_t, void (*)(heatmap_stamp_t*)> getStamp(unsigned radius);

  boost::optional<double> resolution;
  boost::optional<double> radius;
  boost::optional<std::string> kernel;
  boost::optional<double> deviation;

  unsigned max_points;                          // Configured or default max heatmap size
  const unsigned max_max_points = 2000 * 2000;  // Max allowed heatmap size

 private:
};  // class Heatmap

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
