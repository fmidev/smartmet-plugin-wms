#pragma once

#include <algorithm>
#include <optional>
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
  ParameterInfo() = default;
  explicit ParameterInfo(std::string p) : parameter(std::move(p)) {}
  std::size_t hash_value() const;
  std::string to_string() const;

  std::string parameter;
  std::optional<std::string> source;
  std::optional<std::string> producer;
  std::optional<int> forecastType;
  std::optional<int> forecastNumber;
  std::optional<uint> geometryId;
  std::optional<int> levelId;
  std::optional<double> level;
  std::optional<double> pressure;
  std::optional<std::string> elevation_unit;
};

using ParameterInfos = std::vector<ParameterInfo>;

bool operator==(const ParameterInfo& first, const ParameterInfo& second);

// Add to end of the list unless its a duplicate
bool add(ParameterInfos& infos, const ParameterInfo& info);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
