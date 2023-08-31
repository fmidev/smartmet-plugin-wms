// ======================================================================
/*!
 * \brief Class to prioritize points with value
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <spine/Json.h>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class PointData;

struct PointValueOptions
{
  void init(Json::Value& theJson);
  boost::optional<double> mindistance;
  boost::optional<std::string> priority;
  boost::optional<std::vector<int>> priorities;
  std::string rendering_order = "normal";

  std::size_t hash_value() const;
};

std::vector<PointData> prioritize(const std::vector<PointData>& pv, const PointValueOptions& opts);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
