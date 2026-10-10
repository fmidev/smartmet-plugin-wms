// ======================================================================
/*!
 * \brief Class to prioritize points with value
 */
// ======================================================================

#pragma once

#include <spine/Json.h>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class PointData;

// A priority list used when the value of the given parameter is within the limits.
// Without a parameter the list is used unconditionally.
struct ConditionalPriorities
{
  std::optional<std::string> parameter;
  std::optional<double> lolimit;
  std::optional<double> hilimit;
  std::vector<int> priorities;

  bool accepts(double theValue) const;
  std::size_t hash_value() const;
};

struct PointValueOptions
{
  void init(Json::Value& theJson);
  std::optional<double> mindistance;
  std::optional<std::string> priority;
  std::optional<std::vector<int>> priorities;
  std::vector<ConditionalPriorities> conditional_priorities;
  std::string rendering_order = "normal";

  // Extra parameters needed by the conditional priorities
  std::vector<std::string> extraParameters() const;

  std::size_t hash_value() const;
};

// The extra parameters, if any, must be in the point data columns starting from the given index
std::vector<PointData> prioritize(const std::vector<PointData>& pv,
                                  const PointValueOptions& opts,
                                  std::size_t first_extra_column = 1);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
