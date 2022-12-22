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
#include <boost/optional.hpp>
#include <macgyver/Exception.h>

#include <list>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSTimeDimension
{
 public:
  virtual ~WMSTimeDimension() = default;
  WMSTimeDimension() : current(true) {}

  virtual bool isValidTime(const boost::posix_time::ptime& theTime) const;

  virtual boost::posix_time::ptime mostCurrentTime() const;

  std::set<boost::posix_time::ptime> getTimeSteps() const;

  virtual std::string getCapabilities(bool multiple_intervals,
                                      const boost::optional<std::string>& starttime,
                                      const boost::optional<std::string>& endtime) const = 0;

  bool currentValue() const { return current; }

 protected:
  bool current = false;
  std::set<boost::posix_time::ptime> itsTimesteps;
  std::string itsCapabilities;  // returned when querying all times
};

class StepTimeDimension : public WMSTimeDimension
{
 public:
  ~StepTimeDimension() override = default;
  StepTimeDimension() = delete;
  StepTimeDimension(const std::list<boost::posix_time::ptime>& times);
  StepTimeDimension(const std::vector<boost::posix_time::ptime>& times);
  StepTimeDimension(const std::set<boost::posix_time::ptime>& times);

  std::string getCapabilities(bool multiple_intervals,
                              const boost::optional<std::string>& starttime,
                              const boost::optional<std::string>& endtime) const override;

 private:
  std::string makeCapabilities(const boost::optional<std::string>& starttime,
                               const boost::optional<std::string>& endtime) const;
};

struct tag_interval
{
  boost::posix_time::ptime startTime = boost::posix_time::not_a_date_time;
  boost::posix_time::ptime endTime = boost::posix_time::not_a_date_time;
  boost::posix_time::time_duration resolution = boost::posix_time::minutes(1);
  tag_interval(const boost::posix_time::ptime& start,
               const boost::posix_time::ptime& end,
               const boost::posix_time::time_duration& res)
      : startTime(start), endTime(end), resolution(res)
  {
  }
  tag_interval(const tag_interval& interval) = default;
};

class IntervalTimeDimension : public WMSTimeDimension
{
 public:
  ~IntervalTimeDimension() override = default;
  IntervalTimeDimension() = delete;
  IntervalTimeDimension(const std::vector<tag_interval>& intervals);

  const std::vector<tag_interval>& getIntervals() const;

  std::string getCapabilities(bool multiple_intervals,
                              const boost::optional<std::string>& starttime,
                              const boost::optional<std::string>& endtime) const override;
  boost::posix_time::ptime mostCurrentTime() const override;
  bool isValidTime(const boost::posix_time::ptime& theTime) const override;

 private:
  std::string makeCapabilitiesTimesteps(const boost::optional<std::string>& starttime,
                                        const boost::optional<std::string>& endtime) const;

  std::string makeCapabilities(const boost::optional<std::string>& starttime,
                               const boost::optional<std::string>& endtime) const;
  std::string getIntervalCapability(const tag_interval& interval,
                                    const boost::posix_time::ptime& requested_startt,
                                    const boost::posix_time::ptime& requested_endt) const;

  std::vector<tag_interval> itsIntervals;
};

using time_intervals = std::vector<tag_interval>;
// If interval(s) with even timesteps are detected return vector of intervals,
// otherwise empty vector. For example ECMWF data has 3h timesteps in the beginning
// and 6h timesteps in the end
template <typename Container>
time_intervals get_intervals(const Container& container)
{
  time_intervals ret;

  // If there are at least three timesteps, find out if timesteps are even
  if (container.size() < 2)
    return ret;

  auto iter = container.begin();
  boost::posix_time::ptime interval_start_time = *iter;
  iter++;
  boost::posix_time::ptime interval_end_time = *iter;
  boost::posix_time::time_duration resolution = (interval_end_time - interval_start_time);
  // Add first interval
  ret.emplace_back(interval_start_time, interval_end_time, resolution);
  tag_interval* current_interval = &ret.back();
  iter++;
  for (; iter != container.end(); iter++)
  {
    boost::posix_time::time_duration latest_timestep = (*iter - current_interval->endTime);
    // Timestep length changes
    if (latest_timestep.total_seconds() != current_interval->resolution.total_seconds())
    {
      // At least three timesteps required to acertain even steps
      if ((current_interval->endTime - current_interval->startTime) == current_interval->resolution)
      {
        ret.clear();
        break;
      }
      interval_start_time = current_interval->endTime;
      interval_end_time = *iter;
      resolution = (interval_end_time - interval_start_time);
      // Add new interval
      ret.emplace_back(interval_start_time, interval_end_time, resolution);
      current_interval = &ret.back();
      continue;
    }
    current_interval->endTime = *iter;
  }
  // If latest interval has only two timesteps, we can not use IntervalTimeDimension
  if (!ret.empty() &&
      (current_interval->endTime - current_interval->startTime) == current_interval->resolution)
    ret.clear();

  return ret;
}

class WMSTimeDimensions
{
 public:
  WMSTimeDimensions(
      const std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>>& tdims);
  void addTimeDimension(const boost::posix_time::ptime& origintime,
                        boost::shared_ptr<WMSTimeDimension> td);
  const WMSTimeDimension& getDefaultTimeDimension() const;
  const WMSTimeDimension& getTimeDimension(const boost::posix_time::ptime& origintime) const;
  bool origintimeOK(const boost::posix_time::ptime& origintime) const;
  const std::vector<boost::posix_time::ptime>& getOrigintimes() const;

  bool isValidReferenceTime(const boost::posix_time::ptime& origintime) const;
  bool isValidTime(const boost::posix_time::ptime& t,
                   const boost::optional<boost::posix_time::ptime>& origintime) const;
  boost::posix_time::ptime mostCurrentTime(
      const boost::optional<boost::posix_time::ptime>& origintime) const;
  bool currentValue() const;
  bool isIdentical(const WMSTimeDimensions& td) const;

 private:
  std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>> itsTimeDimensions;
  boost::posix_time::ptime itsDefaultOrigintime{boost::posix_time::not_a_date_time};
  std::vector<boost::posix_time::ptime> itsOrigintimes;
};

std::ostream& operator<<(std::ostream& ost, const WMSTimeDimensions& timeDimensions);
std::ostream& operator<<(std::ostream& ost, const StepTimeDimension& timeDimension);
std::ostream& operator<<(std::ostream& ost, const IntervalTimeDimension& timeDimension);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
