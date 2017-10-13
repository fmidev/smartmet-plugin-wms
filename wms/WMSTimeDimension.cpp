#include "WMSTimeDimension.h"

#include <boost/algorithm/string/join.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
void WMSTimeDimension::addTimestep(const boost::posix_time::ptime& timestep)
{
  try
  {
    itsTimesteps.insert(timestep);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to add time step!", NULL);
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
    throw Spine::Exception(BCP, "Failed to remove time step!", NULL);
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
    throw Spine::Exception(BCP, "Failed to validate time!", NULL);
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
    throw Spine::Exception(BCP, "Failed to establish most current time!", NULL);
  }
}

std::string StepTimeDimension::getCapabilities() const
{
  try
  {
    std::ostringstream os;

    std::vector<std::string> ret;
    ret.reserve(itsTimesteps.size());

    for (auto& step : itsTimesteps)
    {
      ret.push_back(to_iso_extended_string(step) + "Z");
    }

    return boost::algorithm::join(ret, ", ");
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to extract time dimension capabilities!", NULL);
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

std::string IntervalTimeDimension::getCapabilities() const
{
  try
  {
    std::ostringstream os;

    os << to_iso_extended_string(itsInterval.startTime) << "Z/"
       << to_iso_extended_string(itsInterval.endTime) << "Z/PT";
    if (itsInterval.resolution.hours() == 0 && itsInterval.resolution.minutes() <= 60)
      os << itsInterval.resolution.minutes() << "M";
    else
    {
      os << itsInterval.resolution.hours() << "H";
      if (itsInterval.resolution.minutes() > 0)
        os << itsInterval.resolution.minutes() << "M";
    }

    return os.str();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to generate time dimension capabilities!", NULL);
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
    throw Spine::Exception(BCP, "Failed to print step time dimension data!", NULL);
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
    throw Spine::Exception(BCP, "Failed to print time dimension data!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
