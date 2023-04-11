// ======================================================================
/*!
 * \brief Sampling details
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <json/json.h>
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
  boost::optional<double> getResolution(const Projection& theProjection) const;

  boost::optional<double> maxresolution;
  boost::optional<double> minresolution;

  boost::optional<double> resolution;
  boost::optional<double> relativeresolution;

 private:
};  // class Sampling

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
