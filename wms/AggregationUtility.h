#pragma once

#include <engines/querydata/Q.h>
#include <macgyver/DateTime.h>
#include <macgyver/LocalDateTime.h>
#include <timeseries/TimeSeries.h>
#include <timeseries/TimeSeriesInclude.h>
#include <optional>
#include <vector>

namespace SmartMet::Engine::Observation
{
class Engine;
class Settings;
}  // namespace SmartMet::Engine::Observation
namespace SmartMet::Engine::Querydata
{
struct ParameterOptions;
}  // namespace SmartMet::Engine::Querydata
namespace SmartMet::TimeSeries
{
struct ParameterAndFunctions;
}  // namespace SmartMet::TimeSeries

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace AggregationUtility
{
TS::Value get_qengine_value(const Engine::Querydata::Q& q,
                            const Engine::Querydata::ParameterOptions& options,
                            const Fmi::LocalDateTime& valid_time,
                            const std::optional<TS::ParameterAndFunctions>& param_funcs);

TS::TimeSeriesVectorPtr get_obsengine_values(
    Engine::Observation::Engine& obsengine,
    const Fmi::DateTime& valid_time,
    const std::vector<TS::ParameterAndFunctions>& paramFuncs,
    unsigned int fmisid_index,
    Engine::Observation::Settings& settings);

}  // namespace AggregationUtility
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
