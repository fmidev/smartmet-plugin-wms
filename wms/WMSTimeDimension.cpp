#include "WMSTimeDimension.h"
#include <boost/algorithm/string/join.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <memory>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{

namespace
{
std::string getIntervalCapability(const tag_interval& interval,
                                  const Fmi::DateTime& requested_startt,
                                  const Fmi::DateTime& requested_endt)
{
  std::string ret;

  auto startt = interval.startTime;
  auto endt = interval.endTime;
  auto resolution =
      (interval.resolution.total_seconds() == 0 ? Fmi::Minutes(1) : interval.resolution);

  if (startt < requested_startt)
  {
    // Time of day taken from interval startTime
    startt = Fmi::DateTime(requested_startt.date(), startt.time_of_day());

    // Starttime adjusted

    while (startt > requested_startt)
      startt -= resolution;
    while (startt < requested_startt)
      startt += resolution;
  }

  // Endtime adjusted
  while (endt > requested_endt)
    endt -= resolution;

  ret = Fmi::to_iso_extended_string(startt) + "Z/" + Fmi::to_iso_extended_string(endt) + "Z/PT";
  if (resolution.hours() == 0 && resolution.minutes() <= 60)
    ret += Fmi::to_string(resolution.minutes()) + "M";
  else
  {
    ret += Fmi::to_string(resolution.hours()) + "H";
    if (resolution.minutes() > 0)
      ret += Fmi::to_string(resolution.minutes()) + "M";
  }

  return ret;
}
}  // namespace

bool WMSTimeDimension::isValidTime(const Fmi::DateTime& theTime,
                                   bool endtime_is_wall_clock_time) const
{
  // Allow any time within the range

  if (itsTimesteps.empty())
    return false;

  auto starttime = *itsTimesteps.cbegin();
  auto endtime =
      (endtime_is_wall_clock_time ? Fmi::SecondClock::universal_time() : *itsTimesteps.crbegin());

  return (theTime >= starttime && theTime <= endtime);

#if 0
  // Allow only listed times
  try
  {
    auto res = itsTimesteps.find(theTime);

    if (res == itsTimesteps.end())
      return false;
      return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to validate time!");
  }
#endif
}

std::set<Fmi::DateTime> WMSTimeDimension::getTimeSteps() const
{
  return itsTimesteps;
}
Fmi::DateTime WMSTimeDimension::mostCurrentTime() const
{
  try
  {
    if (itsTimesteps.empty())
      return Fmi::DateTime::NOT_A_DATE_TIME;

    auto current_time = Fmi::SecondClock::universal_time();

    // if current time is earlier than first timestep -> return first timestep
    if (current_time <= *(itsTimesteps.begin()))
      return *(itsTimesteps.begin());

    auto itLastTimestep = itsTimesteps.end();
    itLastTimestep--;

    // if current time is later than last timestep -> return last timestep
    if (current_time >= *itLastTimestep)
      return *itLastTimestep;

    // otherwise iterate timesteps ans find the closest one
    auto itPrevious = itsTimesteps.begin();
    for (auto it = itsTimesteps.begin(); it != itsTimesteps.end(); ++it)
    {
      if (*it >= current_time)
      {
        // perfect match: current timestep is equal to current time
        if (*it == current_time)
          return *it;

        // check which is closer to current time: current timestep or previous timestep
        auto duration_tonext(*it - current_time);
        auto duration_toprevious(current_time - *itPrevious);

        if (duration_toprevious.total_seconds() <= duration_tonext.total_seconds())
          return *itPrevious;

        return *it;
      }

      itPrevious = it;
    }

    return Fmi::DateTime::NOT_A_DATE_TIME;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to establish most current time!");
  }
}

StepTimeDimension::StepTimeDimension(const std::list<Fmi::DateTime>& times)
{
  std::copy(times.begin(), times.end(), std::inserter(itsTimesteps, itsTimesteps.begin()));
  itsCapabilities = makeCapabilities(std::nullopt, std::nullopt);
}

StepTimeDimension::StepTimeDimension(const std::vector<Fmi::DateTime>& times)
{
  std::copy(times.begin(), times.end(), std::inserter(itsTimesteps, itsTimesteps.begin()));
  itsCapabilities = makeCapabilities(std::nullopt, std::nullopt);
}

StepTimeDimension::StepTimeDimension(const std::set<Fmi::DateTime>& times)
{
  std::copy(times.begin(), times.end(), std::inserter(itsTimesteps, itsTimesteps.begin()));
  itsCapabilities = makeCapabilities(std::nullopt, std::nullopt);
}

std::string StepTimeDimension::getCapabilities(bool /* multiple_intervals */,
                                               const std::optional<Fmi::DateTime>& starttime,
                                               const std::optional<Fmi::DateTime>& endtime) const
{
  try
  {
    if (!starttime && !endtime)
      return itsCapabilities;

    return makeCapabilities(starttime, endtime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract time dimension capabilities!");
  }
}

std::string StepTimeDimension::makeCapabilities(const std::optional<Fmi::DateTime>& starttime,
                                                const std::optional<Fmi::DateTime>& endtime) const
{
  try
  {
    std::vector<std::string> ret;
    ret.reserve(itsTimesteps.size());

    auto startt = (starttime ? *starttime : Fmi::DateTime::min);
    auto endt = (endtime ? *endtime : Fmi::DateTime::max);

    for (const auto& step : itsTimesteps)
    {
      if (step >= startt && step <= endt)
        ret.push_back(Fmi::to_iso_extended_string(step) + "Z");
      if (step > endt)
        break;
    }

    return boost::algorithm::join(ret, ",");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to make time dimension capabilities!");
  }
}

IntervalTimeDimension::IntervalTimeDimension(std::vector<tag_interval> intervals)
    : itsIntervals(std::move(intervals))
{
  itsCapabilities = makeCapabilities(std::nullopt, std::nullopt);
}

Fmi::DateTime IntervalTimeDimension::mostCurrentTime() const
{
  auto current_time = Fmi::SecondClock::universal_time();

  Fmi::DateTime ret = Fmi::DateTime::NOT_A_DATE_TIME;

  for (const auto& interval : itsIntervals)
  {
    if (ret >= interval.startTime && ret <= interval.endTime)
    {
      ret = interval.startTime;
      while (ret + interval.resolution <= current_time)
        ret += interval.resolution;
    }
  }

  // If current time is not inside any interval it can be before first or after last
  if (ret.is_not_a_date_time())
  {
    const auto& first_interval = itsIntervals.front();
    if (current_time < first_interval.startTime)
    {
      ret = first_interval.startTime;
    }
    else
    {
      const auto& last_interval = itsIntervals.back();
      ret = last_interval.endTime;
    }
  }

  return ret;
}

bool IntervalTimeDimension::isValidTime(const Fmi::DateTime& theTime,
                                        bool endtime_is_wall_clock_time) const
{
  for (const auto& interval : itsIntervals)
  {
    const auto& starttime = interval.startTime;
    auto endtime =
        (endtime_is_wall_clock_time ? Fmi::SecondClock::universal_time() : interval.endTime);

    // We allow interpolation
    if (theTime >= starttime && theTime <= endtime)
      return true;
  }

  return false;
}

const std::vector<tag_interval>& IntervalTimeDimension::getIntervals() const
{
  return itsIntervals;
}

std::string IntervalTimeDimension::getCapabilities(
    bool multiple_intervals,
    const std::optional<Fmi::DateTime>& starttime,
    const std::optional<Fmi::DateTime>& endtime) const
{
  try
  {
    // Show individual timesteps
    if (!multiple_intervals && itsIntervals.size() > 1)
      return makeCapabilitiesTimesteps(starttime, endtime);

    if (!starttime && !endtime)
      return itsCapabilities;

    return makeCapabilities(starttime, endtime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate time dimension capabilities!");
  }
}

std::string IntervalTimeDimension::makeCapabilities(
    const std::optional<Fmi::DateTime>& starttime,
    const std::optional<Fmi::DateTime>& endtime) const
{
  try
  {
    if ((starttime && endtime) && *starttime > *endtime)
      throw Fmi::Exception::Trace(BCP,
                                  "Requested starttime must be earlier than requested endtime!");

    std::string ret;

    Fmi::DateTime previous_interval_end_time = Fmi::DateTime::NOT_A_DATE_TIME;
    for (const auto& interval : itsIntervals)
    {
      Fmi::TimePeriod interval_period(interval.startTime, interval.endTime);
      auto requested_startt = (starttime ? *starttime : interval.startTime);
      auto requested_endt = (endtime ? *endtime : interval.endTime);
      Fmi::TimePeriod requested_period(requested_startt, requested_endt);

      if (!interval_period.intersects(requested_period))
        continue;

      // If there are several intervals, they must be separate: the next can not start
      // from the same timestep where the previous ends
      tag_interval actual_interval = interval;
      if (previous_interval_end_time == actual_interval.startTime)
      {
        actual_interval.startTime += interval.resolution;
      }
      std::string interval_capa =
          getIntervalCapability(actual_interval, requested_startt, requested_endt);
      if (!interval_capa.empty())
      {
        if (!ret.empty())
          ret += ",";
        ret += interval_capa;
      }
      previous_interval_end_time = interval.endTime;
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to make time dimension capabilities!");
  }
}

std::string IntervalTimeDimension::makeCapabilitiesTimesteps(
    const std::optional<Fmi::DateTime>& starttime,
    const std::optional<Fmi::DateTime>& endtime) const
{
  try
  {
    if ((starttime && endtime) && *starttime > *endtime)
      throw Fmi::Exception::Trace(BCP,
                                  "Requested starttime must be earlier than requested endtime!");

    std::string ret;
    for (const auto& interval : itsIntervals)
    {
      Fmi::TimePeriod interval_period(interval.startTime, interval.endTime);
      auto requested_startt = (starttime ? *starttime : interval.startTime);
      auto requested_endt = (endtime ? *endtime : interval.endTime);
      Fmi::TimePeriod requested_period(requested_startt, requested_endt);

      if (!interval_period.intersects(requested_period))
        continue;

      Fmi::DateTime timestep = interval.startTime;
      while (timestep <= interval.endTime)
      {
        if (requested_period.contains(timestep))
        {
          if (!ret.empty())
            ret += ",";
          ret += (Fmi::to_iso_extended_string(timestep) + "Z");
        }
        timestep += interval.resolution;
      }
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to make time dimension capabilities!");
  }
}

WMSTimeDimensions::WMSTimeDimensions(
    const std::map<Fmi::DateTime, std::shared_ptr<WMSTimeDimension>>& tdims)
{
  for (const auto& td : tdims)
    addTimeDimension(td.first, td.second);
}

void WMSTimeDimensions::addTimeDimension(const Fmi::DateTime& origintime,
                                         const std::shared_ptr<WMSTimeDimension>& td)
{
  itsTimeDimensions[origintime] = td;
  if (itsDefaultOrigintime == Fmi::DateTime::NOT_A_DATE_TIME || itsDefaultOrigintime < origintime)
    itsDefaultOrigintime = origintime;
  itsOrigintimes.push_back(origintime);
}

const WMSTimeDimension& WMSTimeDimensions::getDefaultTimeDimension() const
{
  return *itsTimeDimensions.at(itsDefaultOrigintime);
}

const WMSTimeDimension& WMSTimeDimensions::getTimeDimension(const Fmi::DateTime& origintime) const
{
  return *itsTimeDimensions.at(origintime);
}

bool WMSTimeDimensions::origintimeOK(const Fmi::DateTime& origintime) const
{
  return itsTimeDimensions.find(origintime) != itsTimeDimensions.end();
}

const std::vector<Fmi::DateTime>& WMSTimeDimensions::getOrigintimes() const
{
  return itsOrigintimes;
}

bool WMSTimeDimensions::isValidReferenceTime(const Fmi::DateTime& origintime) const
{
  return (itsTimeDimensions.find(origintime) != itsTimeDimensions.end());
}

bool WMSTimeDimensions::isValidTime(const Fmi::DateTime& t,
                                    const std::optional<Fmi::DateTime>& origintime) const
{
  if (!origintime)
    return getDefaultTimeDimension().isValidTime(t, itsEndTimeIsWallClockTime);

  // Check for valid origintime
  if (!isValidReferenceTime(*origintime))
    return false;

  return getTimeDimension(*origintime).isValidTime(t, itsEndTimeIsWallClockTime);
}

Fmi::DateTime WMSTimeDimensions::mostCurrentTime(
    const std::optional<Fmi::DateTime>& origintime) const
{
  if (origintime)
    return getTimeDimension(*origintime).mostCurrentTime();

  return getDefaultTimeDimension().mostCurrentTime();
}

bool WMSTimeDimensions::currentValue() const
{
  return getDefaultTimeDimension().currentValue();
}

bool WMSTimeDimensions::isIdentical(const WMSTimeDimensions& td) const
{
  for (const auto& item : itsTimeDimensions)
  {
    if (td.itsTimeDimensions.find(item.first) == td.itsTimeDimensions.end())
      return false;

    const std::shared_ptr<WMSTimeDimension>& tdim1 = item.second;
    const std::shared_ptr<WMSTimeDimension>& tdim2 = td.itsTimeDimensions.at(item.first);

    std::optional<Fmi::DateTime> missing_time;
    if (tdim1->getCapabilities(false, missing_time, missing_time) !=
        tdim2->getCapabilities(false, missing_time, missing_time))
      return false;
  }

  return true;
}

std::ostream& operator<<(std::ostream& ost, const StepTimeDimension& timeDimension)
{
  try
  {
    auto timesteps = timeDimension.getTimeSteps();

    for (const auto& step : timesteps)
      ost << step << " ";

    ost << '\n';

    return ost;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to print step time dimension data!");
  }
}

std::ostream& operator<<(std::ostream& ost, const IntervalTimeDimension& timeDimension)
{
  try
  {
    auto intervals = timeDimension.getIntervals();
    for (const auto& interval : intervals)
      ost << interval.startTime << " - " << interval.endTime << " : " << interval.resolution
          << '\n';

    return ost;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to print time dimension data!");
  }
}

std::ostream& operator<<(std::ostream& ost, const WMSTimeDimensions& timeDimensions)
{
  try
  {
    const std::vector<Fmi::DateTime>& origintimes = timeDimensions.getOrigintimes();

    for (const auto& ot : origintimes)
    {
      if (!ot.is_not_a_date_time())
        ost << "Time dimension for origintime " << ot << " -> \n";
      const WMSTimeDimension* td = &timeDimensions.getTimeDimension(ot);
      if (dynamic_cast<const IntervalTimeDimension*>(td) != nullptr)
        ost << *(dynamic_cast<const IntervalTimeDimension*>(td)) << '\n';
      else if (dynamic_cast<const StepTimeDimension*>(td) != nullptr)
        ost << *(dynamic_cast<const StepTimeDimension*>(td)) << '\n';
    }

    return ost;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to print time dimension data!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
