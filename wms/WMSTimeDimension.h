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

  WMSTimeDimension(const WMSTimeDimension& other) = delete;
  WMSTimeDimension(WMSTimeDimension&& other) = delete;
  WMSTimeDimension& operator=(const WMSTimeDimension& other) = delete;
  WMSTimeDimension& operator=(WMSTimeDimension&& other) = delete;

  virtual bool isValidTime(const boost::posix_time::ptime& theTime, bool endtime_is_wall_clock_time) const;

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
  explicit StepTimeDimension(const std::list<boost::posix_time::ptime>& times);
  explicit StepTimeDimension(const std::vector<boost::posix_time::ptime>& times);
  explicit StepTimeDimension(const std::set<boost::posix_time::ptime>& times);

  StepTimeDimension(const StepTimeDimension& other) = delete;
  StepTimeDimension(StepTimeDimension&& other) = delete;
  StepTimeDimension& operator=(const StepTimeDimension& other) = delete;
  StepTimeDimension& operator=(StepTimeDimension&& other) = delete;

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

  tag_interval(boost::posix_time::ptime start,
               boost::posix_time::ptime end,
               boost::posix_time::time_duration res)
      : startTime(start), endTime(end), resolution(std::move(res))
  {
  }
};

class IntervalTimeDimension : public WMSTimeDimension
{
 public:
  ~IntervalTimeDimension() override = default;
  IntervalTimeDimension() = delete;
  explicit IntervalTimeDimension(std::vector<tag_interval> intervals);

  IntervalTimeDimension(const IntervalTimeDimension& other) = delete;
  IntervalTimeDimension(IntervalTimeDimension&& other) = delete;
  IntervalTimeDimension& operator=(const IntervalTimeDimension& other) = delete;
  IntervalTimeDimension& operator=(IntervalTimeDimension&& other) = delete;

  const std::vector<tag_interval>& getIntervals() const;

  std::string getCapabilities(bool multiple_intervals,
                              const boost::optional<std::string>& starttime,
                              const boost::optional<std::string>& endtime) const override;
  boost::posix_time::ptime mostCurrentTime() const override;
  bool isValidTime(const boost::posix_time::ptime& theTime, bool endtime_is_wall_clock_time) const override;

 private:
  std::string makeCapabilitiesTimesteps(const boost::optional<std::string>& starttime,
                                        const boost::optional<std::string>& endtime) const;

  std::string makeCapabilities(const boost::optional<std::string>& starttime,
                               const boost::optional<std::string>& endtime) const;

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
    // If endTime missing set it and continue (this happens when resolution has changed)
    if (current_interval->endTime.is_not_a_date_time())
    {
      current_interval->endTime = *iter;
      current_interval->resolution = (current_interval->endTime - current_interval->startTime);
      continue;
    }
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
      // Set startTime to iter and endTime to not_a_date_time (endTime set on the next round)
      current_interval->startTime = *iter;
      current_interval->endTime = boost::posix_time::not_a_date_time;
      continue;
    }
    current_interval->endTime = *iter;
  }
  // If latest interval has only two timesteps, we can not use IntervalTimeDimension
  if (!ret.empty() &&
      (current_interval->endTime.is_not_a_date_time() ||
       (current_interval->endTime - current_interval->startTime) == current_interval->resolution))
    ret.clear();

  return ret;
}

class WMSTimeDimensions
{
 public:
  explicit WMSTimeDimensions(
      const std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>>& tdims);
  void addTimeDimension(const boost::posix_time::ptime& origintime,
                        const boost::shared_ptr<WMSTimeDimension>& td);
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
  void useWallClockTimeAsEndTime(bool wall_clock = true) { itsEndTimeIsWallClockTime = wall_clock; }
  bool endTimeFromWallClock() const { return itsEndTimeIsWallClockTime; }

 private:
  std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>> itsTimeDimensions;
  boost::posix_time::ptime itsDefaultOrigintime{boost::posix_time::not_a_date_time};
  std::vector<boost::posix_time::ptime> itsOrigintimes;
  bool itsEndTimeIsWallClockTime{false};
};

std::ostream& operator<<(std::ostream& ost, const WMSTimeDimensions& timeDimensions);
std::ostream& operator<<(std::ostream& ost, const StepTimeDimension& timeDimension);
std::ostream& operator<<(std::ostream& ost, const IntervalTimeDimension& timeDimension);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
