#pragma once

#include <optional>
#include <algorithm>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct ParameterInfo
{
  explicit ParameterInfo(std::string p) : parameter(std::move(p)) {}

  std::string parameter;
  std::optional<std::string> producer;
  std::optional<double> level;

#if 0
  std::optional<int> levelId;
  std::optional<int> forecastType;
  std::optional<int> forecastNumber;
  std::optional<int> geometryId;
#endif
};

using ParameterInfos = std::vector<ParameterInfo>;

bool operator==(const ParameterInfo& first, const ParameterInfo& second);

// Add to end of the list unless its a duplicate
bool add(ParameterInfos& infos, const ParameterInfo& info);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
