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
#include <macgyver/Exception.h>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
class TimeDimension
{
 public:
  virtual ~TimeDimension() = default;
  TimeDimension() : current(true) {}

  TimeDimension(const TimeDimension& other) = delete;
  TimeDimension(TimeDimension&& other) = delete;
  TimeDimension& operator=(const TimeDimension& other) = delete;
  TimeDimension& operator=(TimeDimension&& other) = delete;

  virtual bool isValidTime(const Fmi::DateTime& theTime, bool endtime_is_wall_clock_time) const;

  virtual Fmi::DateTime mostCurrentTime() const;

  // First and last available time, NOT_A_DATE_TIME when there is none
  virtual Fmi::DateTime firstTime() const;
  virtual Fmi::DateTime lastTime() const;

  std::set<Fmi::DateTime> getTimeSteps() const;

  virtual std::string getCapabilities(bool multiple_intervals,
                                      const std::optional<Fmi::DateTime>& starttime,
                                      const std::optional<Fmi::DateTime>& endtime) const = 0;

  bool currentValue() const { return current; }

 protected:
  bool current = false;
  std::set<Fmi::DateTime> itsTimesteps;
  std::string itsCapabilities;  // returned when querying all times
};

class StepTimeDimension : public TimeDimension
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
                              const std::optional<Fmi::DateTime>& starttime,
                              const std::optional<Fmi::DateTime>& endtime) const override;

 private:
  std::string makeCapabilities(const std::optional<Fmi::DateTime>& starttime,
                               const std::optional<Fmi::DateTime>& endtime) const;
};

struct tag_interval
{
  Fmi::DateTime startTime = Fmi::DateTime::NOT_A_DATE_TIME;
  Fmi::DateTime endTime = Fmi::DateTime::NOT_A_DATE_TIME;
  Fmi::TimeDuration resolution = Fmi::Minutes(1);

  tag_interval(const Fmi::DateTime& start, const Fmi::DateTime& end, const Fmi::TimeDuration& res)
      : startTime(start), endTime(end), resolution(res)
  {
  }
};

class IntervalTimeDimension : public TimeDimension
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
                              const std::optional<Fmi::DateTime>& starttime,
                              const std::optional<Fmi::DateTime>& endtime) const override;
  Fmi::DateTime mostCurrentTime() const override;
  Fmi::DateTime firstTime() const override;
  Fmi::DateTime lastTime() const override;
  bool isValidTime(const Fmi::DateTime& theTime, bool endtime_is_wall_clock_time) const override;

 private:
  std::string makeCapabilitiesTimesteps(const std::optional<Fmi::DateTime>& starttime,
                                        const std::optional<Fmi::DateTime>& endtime) const;

  std::string makeCapabilities(const std::optional<Fmi::DateTime>& starttime,
                               const std::optional<Fmi::DateTime>& endtime) const;

  std::vector<tag_interval> itsIntervals;
};

using time_intervals = std::vector<tag_interval>;

// Split the times into intervals of even timesteps. Both a change in the timestep (for example
// ECMWF 1h -> 3h -> 6h) and a gap of missing times (for example a missing radar frame) start a
// new interval. A run of two times is an interval too, a lone time between two gaps is an
// interval with startTime == endTime. Returns an empty vector if no run has at least three times,
// since a plain list of times is then just as compact.
time_intervals make_intervals(std::vector<Fmi::DateTime> times);

template <typename Container>
time_intervals get_intervals(const Container& container)
{
  return make_intervals(std::vector<Fmi::DateTime>(container.begin(), container.end()));
}

// Interval time dimension if the times form intervals, otherwise a step time dimension
std::shared_ptr<TimeDimension> make_time_dimension(const std::list<Fmi::DateTime>& times);
std::shared_ptr<TimeDimension> make_time_dimension(const std::vector<Fmi::DateTime>& times);

class TimeDimensions
{
 public:
  explicit TimeDimensions(
      const std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>>& tdims);
  void addTimeDimension(const Fmi::DateTime& origintime,
                        const std::shared_ptr<TimeDimension>& td);
  const TimeDimension& getDefaultTimeDimension() const;
  const TimeDimension& getTimeDimension(const Fmi::DateTime& origintime) const;
  bool origintimeOK(const Fmi::DateTime& origintime) const;
  const std::vector<Fmi::DateTime>& getOrigintimes() const;

  bool isValidReferenceTime(const Fmi::DateTime& origintime) const;
  bool isValidTime(const Fmi::DateTime& t, const std::optional<Fmi::DateTime>& origintime) const;
  // The default time of the layer, used when a request names none:
  //  - observation-like data (useLatestTimeAsDefault): the latest available time
  //  - forecasts: the time closest to the wall clock, or the first time when
  //    the whole forecast is in the past, so historical data is browsed from
  //    its beginning and current data from now
  Fmi::DateTime mostCurrentTime(const std::optional<Fmi::DateTime>& origintime) const;
  bool currentValue() const;
  bool isIdentical(const TimeDimensions& td) const;
  void useWallClockTimeAsEndTime(bool wall_clock = true) { itsEndTimeIsWallClockTime = wall_clock; }
  bool endTimeFromWallClock() const { return itsEndTimeIsWallClockTime; }
  void useLatestTimeAsDefault(bool latest = true) { itsLatestTimeIsDefault = latest; }
  bool latestTimeIsDefault() const { return itsLatestTimeIsDefault; }

 private:
  std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>> itsTimeDimensions;
  Fmi::DateTime itsDefaultOrigintime{Fmi::DateTime::NOT_A_DATE_TIME};
  std::vector<Fmi::DateTime> itsOrigintimes;
  bool itsEndTimeIsWallClockTime{false};
  bool itsLatestTimeIsDefault{false};
};

std::ostream& operator<<(std::ostream& ost, const TimeDimensions& timeDimensions);
std::ostream& operator<<(std::ostream& ost, const StepTimeDimension& timeDimension);
std::ostream& operator<<(std::ostream& ost, const IntervalTimeDimension& timeDimension);

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
