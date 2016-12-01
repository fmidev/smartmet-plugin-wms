// ======================================================================
/*!
 * \brief A Web Map Service time dimension data structure
 *
 * Characteristics:
 *
 *  - Times must be given in UTC timezone
 *  -
 */
// ======================================================================

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <spine/Exception.h>

#include <set>
#include <list>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSTimeDimension
{
 public:
  WMSTimeDimension() : current(true) {}
  void addTimestep(const boost::posix_time::ptime& timestep);

  void removeTimestep(const boost::posix_time::ptime& timestep);

  bool isValidTime(const boost::posix_time::ptime& theTime) const;

  boost::posix_time::ptime mostCurrentTime() const;

  std::set<boost::posix_time::ptime> getTimeSteps() const;

  virtual std::string getCapabilities() const = 0;

  bool currentValue() { return current; }
 protected:
  bool current;
  std::set<boost::posix_time::ptime> itsTimesteps;
};

class StepTimeDimension : public WMSTimeDimension
{
 public:
  StepTimeDimension() = default;

  virtual std::string getCapabilities() const;
};

class IntervalTimeDimension : public WMSTimeDimension
{
  struct tag_interval
  {
    boost::posix_time::ptime startTime = boost::posix_time::not_a_date_time;
    boost::posix_time::ptime endTime = boost::posix_time::not_a_date_time;
    ;
    boost::posix_time::time_duration resolution = boost::posix_time::minutes(1);
    tag_interval(const boost::posix_time::ptime& start,
                 const boost::posix_time::ptime& end,
                 const boost::posix_time::time_duration& res)
        : startTime(start), endTime(end), resolution(res)
    {
    }
  };

 public:
  IntervalTimeDimension(const boost::posix_time::ptime& start,
                        const boost::posix_time::ptime& end,
                        const boost::posix_time::time_duration& res);

  tag_interval getInterval() const;

  virtual std::string getCapabilities() const;

 private:
  tag_interval itsInterval;
};

std::ostream& operator<<(std::ostream& ost, const StepTimeDimension& timeDimension);
std::ostream& operator<<(std::ostream& ost, const IntervalTimeDimension& timeDimension);

// check if timesteps are even, timesteps are in a list or in a vector
template <typename Container>
bool even_timesteps(const Container& container)
{
  bool even_timesteps(true);

  // if at least three timesteps, find out if timesteps are even
  if (container.size() < 2)
    return false;

  auto iter = container.begin();
  boost::posix_time::ptime previous_time(*iter);
  iter++;
  boost::posix_time::time_duration first_timestep = (*iter - previous_time);
  for (; iter != container.end(); iter++)
  {
    boost::posix_time::time_duration latest_timestep = (*iter - previous_time);
    if (latest_timestep.total_seconds() != first_timestep.total_seconds())
    {
      even_timesteps = false;
      break;
    }
    previous_time = *iter;
  }

  return even_timesteps;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
