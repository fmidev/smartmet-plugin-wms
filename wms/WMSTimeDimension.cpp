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

void WMSTimeDimension::addTimestep(const boost::posix_time::ptime& timestep)
{
  try
  {
    itsTimesteps.insert(timestep);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to add time step!");
  }
}

void WMSTimeDimension::removeTimestep(const boost::posix_time::ptime& timestep)
{
  try
  {
    itsTimesteps.erase(timestep);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to remove time step!");
  }
}

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
    {
      return false;
    }
    else
    {
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to validate time!");
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
    if (itsTimesteps.size() == 0)
      return boost::posix_time::not_a_date_time;

    boost::posix_time::ptime current_time = boost::posix_time::second_clock::universal_time();

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
        boost::posix_time::time_duration duration_tonext(*it - current_time);
        boost::posix_time::time_duration duration_toprevious(current_time - *itPrevious);

        if (duration_toprevious.total_seconds() <= duration_tonext.total_seconds())
          return *itPrevious;
        else
          return *it;
      }

      itPrevious = it;
    }

    return boost::posix_time::not_a_date_time;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to establish most current time!");
  }
}

std::string StepTimeDimension::getCapabilities(const boost::optional<std::string>& starttime,
                                               const boost::optional<std::string>& endtime) const
{
  try
  {
    std::vector<std::string> ret;
    ret.reserve(itsTimesteps.size());

    boost::posix_time::ptime startt =
        (starttime ? parse_time(*starttime) : boost::posix_time::min_date_time);
    boost::posix_time::ptime endt =
        (endtime ? parse_time(*endtime) : boost::posix_time::max_date_time);

    for (auto& step : itsTimesteps)
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
    throw Spine::Exception::Trace(BCP, "Failed to extract time dimension capabilities!");
  }
}

IntervalTimeDimension::IntervalTimeDimension(const boost::posix_time::ptime& begin,
                                             const boost::posix_time::ptime& end,
                                             const boost::posix_time::time_duration& step)
    : itsInterval(begin, end, step)
{
}

boost::posix_time::ptime IntervalTimeDimension::mostCurrentTime() const
{
  return itsInterval.endTime;
}

bool IntervalTimeDimension::isValidTime(const boost::posix_time::ptime& theTime) const
{
  return (theTime >= itsInterval.startTime && theTime <= itsInterval.endTime);
}

IntervalTimeDimension::tag_interval IntervalTimeDimension::getInterval() const
{
  return itsInterval;
}

std::string IntervalTimeDimension::getCapabilities(
    const boost::optional<std::string>& starttime,
    const boost::optional<std::string>& endtime) const
{
  try
  {
    if ((starttime && endtime) && parse_time(*starttime) > parse_time(*endtime))
      throw Spine::Exception::Trace(BCP,
                                    "Requested starttime must be earlier than requested endtime!");

    boost::posix_time::ptime startt = itsInterval.startTime;
    boost::posix_time::ptime endt = itsInterval.endTime;
    boost::posix_time::ptime requested_startt =
        (starttime ? parse_time(*starttime) : itsInterval.startTime);
    boost::posix_time::ptime requested_endt =
        (endtime ? parse_time(*endtime) : itsInterval.endTime);

    if (requested_startt > endt || requested_endt < startt)
      return "";

    if (startt < requested_startt)
    {
      startt = boost::posix_time::ptime(requested_startt.date(), startt.time_of_day());
      boost::posix_time::time_duration resolution =
          (itsInterval.resolution.total_seconds() == 0 ? boost::posix_time::minutes(1)
                                                       : itsInterval.resolution);
      if (startt < requested_startt)
      {
        while (startt < requested_startt)
          startt += resolution;
      }
      else if (startt > requested_startt)
      {
        while (startt - resolution >= requested_startt)
          startt -= resolution;
      }
    }

    if (requested_endt < endt)
    {
      endt = requested_endt;
    }

    std::string ret =
        Fmi::to_iso_extended_string(startt) + "Z/" + Fmi::to_iso_extended_string(endt) + "Z/PT";
    if (itsInterval.resolution.hours() == 0 && itsInterval.resolution.minutes() <= 60)
      ret += Fmi::to_string(itsInterval.resolution.minutes()) + "M";
    else
    {
      ret += Fmi::to_string(itsInterval.resolution.hours()) + "H";
      if (itsInterval.resolution.minutes() > 0)
        ret += Fmi::to_string(itsInterval.resolution.minutes()) + "M";
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to generate time dimension capabilities!");
  }
}

std::ostream& operator<<(std::ostream& ost, const StepTimeDimension& timeDimension)
{
  try
  {
    auto timesteps = timeDimension.getTimeSteps();

    for (auto& step : timesteps)
    {
      ost << step << " ";
    }

    ost << std::endl;

    return ost;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to print step time dimension data!");
  }
}

std::ostream& operator<<(std::ostream& ost, const IntervalTimeDimension& timeDimension)
{
  try
  {
    auto interval = timeDimension.getInterval();
    ost << interval.startTime << " - " << interval.endTime << " : " << interval.resolution << " ";
    ost << std::endl;

    return ost;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to print time dimension data!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
