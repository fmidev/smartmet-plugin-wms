#pragma once

#include <boost/optional.hpp>
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
  ParameterInfo(std::string p) : parameter(std::move(p)) {}

  std::string parameter;
  boost::optional<std::string> producer;
  boost::optional<double> level;

#if 0
  boost::optional<int> levelId;
  boost::optional<int> forecastType;
  boost::optional<int> forecastNumber;
  boost::optional<int> geometryId;
#endif
};

using ParameterInfos = std::vector<ParameterInfo>;

bool operator==(const ParameterInfo& first, const ParameterInfo& second);

// Add to end of the list unless its a duplicate
bool add(ParameterInfos& infos, const ParameterInfo& info);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
