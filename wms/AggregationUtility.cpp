#include "AggregationUtility.h"
#include <timeseries/ParameterTools.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace AggregationUtility
{
TS::TimeSeriesGenerator::LocalTimeList get_tlist(
    const std::list<Fmi::DateTime>& qengine_times,
    const Fmi::DateTime& valid_time,
    unsigned int interval_behind,
    unsigned int interval_ahead)
{
  try
  {
    auto interval_start = valid_time;
    auto interval_end = valid_time;
    if (interval_behind > 0)
      interval_start -= Fmi::Minutes(interval_behind);
    if (interval_ahead > 0)
      interval_end += Fmi::Minutes(interval_ahead);

    TS::TimeSeriesGenerator::LocalTimeList tlist;
    Fmi::TimeZonePtr utc_zone(new boost::local_time::posix_time_zone("UTC"));

    bool add_valid_time = true;  // Flag to make sure valid_time is not added twice
    for (const auto& t : qengine_times)
    {
      if (t >= interval_start && t <= interval_end)
      {
        if (t >= valid_time && add_valid_time)
        {
          tlist.emplace_back(valid_time, utc_zone);
          add_valid_time = false;
          continue;
        }
        tlist.emplace_back(t, utc_zone);
      }
    }

    return tlist;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::Value get_qengine_value(const Engine::Querydata::Q& q,
                            const Engine::Querydata::ParameterOptions& options,
                            const Fmi::LocalDateTime& valid_time,
                            const boost::optional<TS::ParameterAndFunctions>& param_funcs)
{
  try
  {
    TS::Value result;

    bool do_aggregation = (param_funcs && param_funcs->functions.innerFunction.exists());

    if (do_aggregation)
    {
      auto qengine_times = q->validTimes();
      auto interval_behind = param_funcs->functions.innerFunction.getAggregationIntervalBehind();
      auto interval_ahead = param_funcs->functions.innerFunction.getAggregationIntervalAhead();
      auto tlist =
          get_tlist(*qengine_times, valid_time.utc_time(), interval_behind, interval_ahead);
      // QEngine query
      auto raw_data = q->values(options, tlist);
      // Aggregate data
      auto aggregated_data = TS::Aggregator::aggregate(*raw_data, param_funcs->functions);
      // Remove redundant timesteps, only one timstep is valid for a map
      tlist.clear();
      tlist.push_back(valid_time);
      auto final_data = TS::erase_redundant_timesteps(aggregated_data, tlist);

      // Only one timestep remains
      result = final_data->front().value;
    }
    else
    {
      result = q->value(options, valid_time);
    }

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// OBS ENGINE

bool set_aggregation_period(Engine::Observation::Settings& settings,
                            const std::vector<TS::ParameterAndFunctions>& paramFuncs)
{
  try
  {
    if (paramFuncs.empty())
      return false;

    unsigned int aggregationIntervalBehind = 0;
    unsigned int aggregationIntervalAhead = 0;
    for (const auto& paramAndFunctions : paramFuncs)
    {
      const auto& funcs = paramAndFunctions.functions;
      if (funcs.innerFunction.exists())
      {
        aggregationIntervalBehind =
            std::max(aggregationIntervalBehind, funcs.innerFunction.getAggregationIntervalBehind());
        aggregationIntervalAhead =
            std::max(aggregationIntervalAhead, funcs.innerFunction.getAggregationIntervalAhead());
      }
    }

    if (aggregationIntervalBehind == 0 && aggregationIntervalAhead == 0)
      return false;

    if (aggregationIntervalBehind > 0)
      settings.starttime =
          settings.starttime - Fmi::Minutes(aggregationIntervalBehind);
    if (aggregationIntervalAhead > 0)
      settings.endtime = settings.endtime + Fmi::Minutes(aggregationIntervalAhead);
    // Read all observartions
    settings.wantedtime = boost::optional<Fmi::DateTime>();
    settings.timestep = 0;

    return (aggregationIntervalBehind > 0 || aggregationIntervalAhead > 0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_location_parameter(const std::string& paramname)
{
  try
  {
    return (paramname == "stationlon" || paramname == "stationlat" || paramname == "fmisid" ||
            TS::is_location_parameter(paramname));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void fill_missing_location_params(TS::TimeSeries& ts)
{
  try
  {
    TS::Value missing_value = TS::None();
    TS::Value actual_value = missing_value;
    bool missing_values_exists = false;
    for (const auto& item : ts)
    {
      if (item.value == missing_value)
        missing_values_exists = true;
      else
        actual_value = item.value;
      if (actual_value != missing_value && missing_values_exists)
        break;
    }
    if (actual_value != missing_value && missing_values_exists)
    {
      for (auto& item : ts)
        if (item.value == missing_value)
          item.value = actual_value;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesVectorPtr prepare_data_for_aggregation(
    const std::string& producer,
    const std::vector<Spine::Parameter> parameters,
    const TS::TimeSeriesVectorPtr& observation_result,
    std::map<std::string, unsigned int>& parameterResultIndexes)
{
  try
  {
    TS::TimeSeriesVectorPtr ret(new TS::TimeSeriesVector());
    unsigned int obs_result_field_index = 0;
    TS::Value missing_value = TS::None();

    // Iterate parameters and store values for all parameters
    // into ret data structure
    for (const auto & parameter : parameters)
    {
      const auto& paramname = parameter.name();
      bool is_location_p = is_location_parameter(paramname);

      // add data fields fetched from observation
      auto result = *observation_result;
      if (result[obs_result_field_index].empty())
        continue;

      auto result_at_index = result[obs_result_field_index];

      // If time location parameter contains missing values in some timesteps,
      // replace them with existing values to keep aggregation working
      if (is_location_p)
        fill_missing_location_params(result_at_index);

      ret->push_back(result_at_index);
      std::string pname_plus_snumber = TS::get_parameter_id(parameter);
      parameterResultIndexes.insert(std::make_pair(pname_plus_snumber, ret->size() - 1));
      obs_result_field_index++;
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesVectorPtr do_aggregation(
    const TS::LocalTimePoolPtr& localTimePool,
    const std::vector<TS::ParameterAndFunctions>& paramFuncs,
    const TS::TimeSeriesVectorPtr& observation_result,
    const std::map<std::string, unsigned int>& parameterResultIndexes)
{
  try
  {
    TS::TimeSeriesVectorPtr aggregated_observation_result(new TS::TimeSeriesVector());
    // iterate parameters and do aggregation
    for (const auto& item : paramFuncs)
    {
      std::string paramname = item.parameter.name();
      std::string pname_plus_snumber = TS::get_parameter_id(item.parameter);
      if (parameterResultIndexes.find(pname_plus_snumber) != parameterResultIndexes.end())
        paramname = pname_plus_snumber;
      else if (parameterResultIndexes.find(paramname) == parameterResultIndexes.end())
        continue;

      unsigned int resultIndex = parameterResultIndexes.at(paramname);
      TS::TimeSeries ts = (*observation_result)[resultIndex];
      TS::DataFunctions pfunc = item.functions;
      TS::TimeSeriesPtr tsptr;
      // If inner function exists aggregation happens
      if (pfunc.innerFunction.exists())
      {
        tsptr = TS::Aggregator::aggregate(ts, pfunc);
        if (tsptr->empty())
          continue;
      }
      else
      {
        tsptr = boost::make_shared<TS::TimeSeries>(localTimePool);
        *tsptr = ts;
      }
      aggregated_observation_result->push_back(*tsptr);
    }
    return aggregated_observation_result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesGeneratorCache::TimeList get_tlist(const Fmi::DateTime& valid_time)
{
  try
  {
    // Only one timstep is valid for a map
    Fmi::TimeZonePtr utc_zone(new boost::local_time::posix_time_zone("UTC"));
    TS::TimeSeriesGeneratorOptions toptions;
    toptions.startTime = valid_time;
    toptions.endTime = valid_time;
    toptions.timeStep = 0;
    TS::TimeSeriesGeneratorCache tsc;
    return tsc.generate(toptions, utc_zone);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesVectorPtr aggregate_data(const TS::TimeSeriesVectorPtr& raw_data,
                                       const Engine::Observation::Settings& settings,
                                       const Fmi::DateTime& valid_time,
                                       const std::vector<TS::ParameterAndFunctions>& paramFuncs,
                                       unsigned int fmisid_index)

{
  try
  {
    // Get tlist, it contains only one timestep
    auto tlist = get_tlist(valid_time);

    // Get timesries grouped by fmisd and make sure that all timesteps in tlist are in the result
    // set (some timesteps in some locations may contain None-value)
    TS::TimeSeriesByLocation result_by_location =
        TS::get_timeseries_by_fmisid(settings.stationtype, raw_data, tlist, fmisid_index);

    // Allocate data structure for aggregated data
    TS::TimeSeriesVectorPtr ret = boost::make_shared<TS::TimeSeriesVector>(
        raw_data->size(), TS::TimeSeries(settings.localTimePool));

    // Iterate locations and do aggregation location by location
    for (const auto& observation_result_location : result_by_location)
    {
      const auto& observation_result = observation_result_location.second;
      std::map<std::string, unsigned int> parameterResultIndexes;
      // Prepare data for aggregation
      TS::TimeSeriesVectorPtr prepared_data = prepare_data_for_aggregation(
          settings.stationtype, settings.parameters, observation_result, parameterResultIndexes);

      // Do aggregation
      TS::TimeSeriesVectorPtr aggregated_data =
          do_aggregation(settings.localTimePool, paramFuncs, prepared_data, parameterResultIndexes);

      // Remove redundant timesteps, only one timstep is valid for a map
      auto final_data = TS::erase_redundant_timesteps(aggregated_data, *tlist);

      // Add aggregated data into ret data structure
      for (unsigned int i = 0; i < final_data->size(); i++)
      {
        auto& destination_ts = ret->at(i);
        const auto& source_ts = final_data->at(i);
        destination_ts.insert(destination_ts.end(), source_ts.begin(), source_ts.end());
      }
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::TimeSeriesVectorPtr get_obsengine_values(
    Engine::Observation::Engine& obsengine,
    const Fmi::DateTime& valid_time,
    const std::vector<TS::ParameterAndFunctions>& paramFuncs,
    unsigned int fmisid_index,
    Engine::Observation::Settings& settings)

{
  try
  {
    // Set aggregation period
    bool do_aggregation = set_aggregation_period(settings, paramFuncs);

    auto result = obsengine.values(settings);

    // Do aggregation
    if (do_aggregation)
      result = aggregate_data(result, settings, valid_time, paramFuncs, fmisid_index);

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace AggregationUtility
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
