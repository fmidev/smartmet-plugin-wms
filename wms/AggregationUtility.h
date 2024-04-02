#pragma once

#include "Layer.h"
#include <engines/observation/Engine.h>
#include <engines/querydata/Engine.h>
#include <timeseries/ParameterFactory.h>
#include <timeseries/TimeSeriesInclude.h>

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
                            const boost::optional<TS::ParameterAndFunctions>& param_funcs);

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
