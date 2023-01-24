#include "WMSTimeDimension.h"
#include <boost/algorithm/string/join.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
boost::posix_time::ptime parse_time(const std::string& time_string)
{
  if (time_string == "now")
    return boost::posix_time::second_clock::universal_time();

  return Fmi::TimeParser::parse(time_string);
}
}  // namespace

bool WMSTimeDimension::isValidTime(const boost::posix_time::ptime& theTime) const
{
  // Allow any time within the range

  if (itsTimesteps.empty())
    return false;

  return (theTime >= *itsTimesteps.cbegin() && theTime <= *itsTimesteps.crbegin());

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

std::set<boost::posix_time::ptime> WMSTimeDimension::getTimeSteps() const
{
  return itsTimesteps;
}
boost::posix_time::ptime WMSTimeDimension::mostCurrentTime() const
{
  try
  {
    if (itsTimesteps.empty())
      return boost::posix_time::not_a_date_time;

    auto current_time = boost::posix_time::second_clock::universal_time();

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

    return boost::posix_time::not_a_date_time;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to establish most current time!");
  }
}

StepTimeDimension::StepTimeDimension(const std::list<boost::posix_time::ptime>& times)
{
  std::copy(times.begin(), times.end(), std::inserter(itsTimesteps, itsTimesteps.begin()));
  itsCapabilities = makeCapabilities(boost::none, boost::none);
}

StepTimeDimension::StepTimeDimension(const std::vector<boost::posix_time::ptime>& times)
{
  std::copy(times.begin(), times.end(), std::inserter(itsTimesteps, itsTimesteps.begin()));
  itsCapabilities = makeCapabilities(boost::none, boost::none);
}

StepTimeDimension::StepTimeDimension(const std::set<boost::posix_time::ptime>& times)
{
  std::copy(times.begin(), times.end(), std::inserter(itsTimesteps, itsTimesteps.begin()));
  itsCapabilities = makeCapabilities(boost::none, boost::none);
}

std::string StepTimeDimension::getCapabilities(bool /* multiple_intervals */,
                                               const boost::optional<std::string>& starttime,
                                               const boost::optional<std::string>& endtime) const
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

std::string StepTimeDimension::makeCapabilities(const boost::optional<std::string>& starttime,
                                                const boost::optional<std::string>& endtime) const
{
  try
  {
    std::vector<std::string> ret;
    ret.reserve(itsTimesteps.size());

    auto startt = (starttime ? parse_time(*starttime) : boost::posix_time::min_date_time);
    auto endt = (endtime ? parse_time(*endtime) : boost::posix_time::max_date_time);

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
  itsCapabilities = makeCapabilities(boost::none, boost::none);
}

boost::posix_time::ptime IntervalTimeDimension::mostCurrentTime() const
{
  auto current_time = boost::posix_time::second_clock::universal_time();

  boost::posix_time::ptime ret = boost::posix_time::not_a_date_time;

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

bool IntervalTimeDimension::isValidTime(const boost::posix_time::ptime& theTime) const
{
  for (const auto& interval : itsIntervals)
  {
    if (theTime >= interval.startTime && theTime <= interval.endTime)
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
    const boost::optional<std::string>& starttime,
    const boost::optional<std::string>& endtime) const
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

std::string IntervalTimeDimension::getIntervalCapability(
    const tag_interval& interval,
    const boost::posix_time::ptime& requested_startt,
    const boost::posix_time::ptime& requested_endt) const
{
  std::string ret;

  auto startt = interval.startTime;
  auto endt = interval.endTime;
  auto resolution = (interval.resolution.total_seconds() == 0 ? boost::posix_time::minutes(1)
                                                              : interval.resolution);

  if (startt < requested_startt)
  {
    // Time of day taken from interval startTime
    startt = boost::posix_time::ptime(requested_startt.date(), startt.time_of_day());

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

std::string IntervalTimeDimension::makeCapabilities(
    const boost::optional<std::string>& starttime,
    const boost::optional<std::string>& endtime) const
{
  try
  {
    if ((starttime && endtime) && parse_time(*starttime) > parse_time(*endtime))
      throw Fmi::Exception::Trace(BCP,
                                  "Requested starttime must be earlier than requested endtime!");

    std::string ret;

    boost::posix_time::ptime previous_interval_end_time = boost::posix_time::not_a_date_time;
    for (const auto& interval : itsIntervals)
    {
      boost::posix_time::time_period interval_period(interval.startTime, interval.endTime);
      auto requested_startt = (starttime ? parse_time(*starttime) : interval.startTime);
      auto requested_endt = (endtime ? parse_time(*endtime) : interval.endTime);
      boost::posix_time::time_period requested_period(requested_startt, requested_endt);

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
    const boost::optional<std::string>& starttime,
    const boost::optional<std::string>& endtime) const
{
  try
  {
    if ((starttime && endtime) && parse_time(*starttime) > parse_time(*endtime))
      throw Fmi::Exception::Trace(BCP,
                                  "Requested starttime must be earlier than requested endtime!");

    std::string ret;
    for (const auto& interval : itsIntervals)
    {
      boost::posix_time::time_period interval_period(interval.startTime, interval.endTime);
      auto requested_startt = (starttime ? parse_time(*starttime) : interval.startTime);
      auto requested_endt = (endtime ? parse_time(*endtime) : interval.endTime);
      boost::posix_time::time_period requested_period(requested_startt, requested_endt);

      if (!interval_period.intersects(requested_period))
        continue;

      boost::posix_time::ptime timestep = interval.startTime;
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
    const std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>>& tdims)
{
  for (const auto& td : tdims)
    addTimeDimension(td.first, td.second);
}

void WMSTimeDimensions::addTimeDimension(const boost::posix_time::ptime& origintime,
                                         const boost::shared_ptr<WMSTimeDimension>& td)
{
  itsTimeDimensions[origintime] = td;
  if (itsDefaultOrigintime == boost::posix_time::not_a_date_time ||
      itsDefaultOrigintime < origintime)
    itsDefaultOrigintime = origintime;
  itsOrigintimes.push_back(origintime);
}

const WMSTimeDimension& WMSTimeDimensions::getDefaultTimeDimension() const
{
  return *itsTimeDimensions.at(itsDefaultOrigintime);
}

const WMSTimeDimension& WMSTimeDimensions::getTimeDimension(
    const boost::posix_time::ptime& origintime) const
{
  return *itsTimeDimensions.at(origintime);
}

bool WMSTimeDimensions::origintimeOK(const boost::posix_time::ptime& origintime) const
{
  return itsTimeDimensions.find(origintime) != itsTimeDimensions.end();
}

const std::vector<boost::posix_time::ptime>& WMSTimeDimensions::getOrigintimes() const
{
  return itsOrigintimes;
}

bool WMSTimeDimensions::isValidReferenceTime(const boost::posix_time::ptime& origintime) const
{
  return (itsTimeDimensions.find(origintime) != itsTimeDimensions.end());
}

bool WMSTimeDimensions::isValidTime(
    const boost::posix_time::ptime& t,
    const boost::optional<boost::posix_time::ptime>& origintime) const
{
  if (!origintime)
    return getDefaultTimeDimension().isValidTime(t);

  // Check for valid origintime
  if (!isValidReferenceTime(*origintime))
    return false;

  return getTimeDimension(*origintime).isValidTime(t);
}

boost::posix_time::ptime WMSTimeDimensions::mostCurrentTime(
    const boost::optional<boost::posix_time::ptime>& origintime) const
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

    const boost::shared_ptr<WMSTimeDimension>& tdim1 = item.second;
    const boost::shared_ptr<WMSTimeDimension>& tdim2 = td.itsTimeDimensions.at(item.first);

    boost::optional<std::string> missing_time;
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

    ost << std::endl;

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
      ost << interval.startTime << " - " << interval.endTime << " : " << interval.resolution << " "
          << std::endl;

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
    const std::vector<boost::posix_time::ptime>& origintimes = timeDimensions.getOrigintimes();

    for (const auto& ot : origintimes)
    {
      if (!ot.is_not_a_date_time())
        ost << "Time dimension for origintime " << ot << " -> " << std::endl;
      const WMSTimeDimension* td = &timeDimensions.getTimeDimension(ot);
      if (dynamic_cast<const IntervalTimeDimension*>(td) != nullptr)
        ost << *(dynamic_cast<const IntervalTimeDimension*>(td)) << std::endl;
      else if (dynamic_cast<const StepTimeDimension*>(td) != nullptr)
        ost << *(dynamic_cast<const StepTimeDimension*>(td)) << std::endl;
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
