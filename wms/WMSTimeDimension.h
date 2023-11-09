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

#include <macgyver/DateTime.h>
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

  virtual bool isValidTime(const Fmi::DateTime& theTime, bool endtime_is_wall_clock_time) const;

  virtual Fmi::DateTime mostCurrentTime() const;

  std::set<Fmi::DateTime> getTimeSteps() const;

  virtual std::string getCapabilities(bool multiple_intervals,
                                      const boost::optional<std::string>& starttime,
                                      const boost::optional<std::string>& endtime) const = 0;

  bool currentValue() const { return current; }

 protected:
  bool current = false;
  std::set<Fmi::DateTime> itsTimesteps;
  std::string itsCapabilities;  // returned when querying all times
};

class StepTimeDimension : public WMSTimeDimension
{
 public:
  ~StepTimeDimension() override = default;
  StepTimeDimension() = delete;
  explicit StepTimeDimension(const std::list<Fmi::DateTime>& times);
  explicit StepTimeDimension(const std::vector<Fmi::DateTime>& times);
  explicit StepTimeDimension(const std::set<Fmi::DateTime>& times);

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
  Fmi::DateTime startTime = boost::posix_time::not_a_date_time;
  Fmi::DateTime endTime = boost::posix_time::not_a_date_time;
  Fmi::TimeDuration resolution = Fmi::Minutes(1);

  tag_interval(Fmi::DateTime start,
               Fmi::DateTime end,
               Fmi::TimeDuration res)
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
  Fmi::DateTime mostCurrentTime() const override;
  bool isValidTime(const Fmi::DateTime& theTime, bool endtime_is_wall_clock_time) const override;

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
  Fmi::DateTime interval_start_time = *iter;
  iter++;
  Fmi::DateTime interval_end_time = *iter;
  Fmi::TimeDuration resolution = (interval_end_time - interval_start_time);
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
    Fmi::TimeDuration latest_timestep = (*iter - current_interval->endTime);
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
      const std::map<Fmi::DateTime, boost::shared_ptr<WMSTimeDimension>>& tdims);
  void addTimeDimension(const Fmi::DateTime& origintime,
                        const boost::shared_ptr<WMSTimeDimension>& td);
  const WMSTimeDimension& getDefaultTimeDimension() const;
  const WMSTimeDimension& getTimeDimension(const Fmi::DateTime& origintime) const;
  bool origintimeOK(const Fmi::DateTime& origintime) const;
  const std::vector<Fmi::DateTime>& getOrigintimes() const;

  bool isValidReferenceTime(const Fmi::DateTime& origintime) const;
  bool isValidTime(const Fmi::DateTime& t,
                   const boost::optional<Fmi::DateTime>& origintime) const;
  Fmi::DateTime mostCurrentTime(
      const boost::optional<Fmi::DateTime>& origintime) const;
  bool currentValue() const;
  bool isIdentical(const WMSTimeDimensions& td) const;
  void useWallClockTimeAsEndTime(bool wall_clock = true) { itsEndTimeIsWallClockTime = wall_clock; }
  bool endTimeFromWallClock() const { return itsEndTimeIsWallClockTime; }

 private:
  std::map<Fmi::DateTime, boost::shared_ptr<WMSTimeDimension>> itsTimeDimensions;
  Fmi::DateTime itsDefaultOrigintime{boost::posix_time::not_a_date_time};
  std::vector<Fmi::DateTime> itsOrigintimes;
  bool itsEndTimeIsWallClockTime{false};
};

std::ostream& operator<<(std::ostream& ost, const WMSTimeDimensions& timeDimensions);
std::ostream& operator<<(std::ostream& ost, const StepTimeDimension& timeDimension);
std::ostream& operator<<(std::ostream& ost, const IntervalTimeDimension& timeDimension);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
