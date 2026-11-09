// Unit tests for splitting valid times into WMS time dimension intervals (ogc/TimeDimension.h).
// Missing times (for example missing radar frames) must split the times into several intervals
// instead of degrading the whole time dimension into a list of all the times.

#define BOOST_TEST_MODULE TimeIntervals
#include <boost/test/unit_test.hpp>

#include "ogc/TimeDimension.h"
#include <macgyver/StringConversion.h>
#include <string>
#include <vector>

using namespace SmartMet::Plugin::OGC;

namespace
{
const Fmi::DateTime base(Fmi::Date(2026, 11, 9), Fmi::Hours(0));

// Times as minutes since base
std::vector<Fmi::DateTime> minutes(const std::vector<int>& mins)
{
  std::vector<Fmi::DateTime> ret;
  for (int m : mins)
    ret.push_back(base + Fmi::Minutes(m));
  return ret;
}

// Intervals as "HH:MM/HH:MM/step" with step in minutes, lone times as "HH:MM"
std::string describe(const time_intervals& intervals)
{
  auto hhmm = [](const Fmi::DateTime& t) { return Fmi::to_iso_extended_string(t).substr(11, 5); };

  std::string ret;
  for (const auto& interval : intervals)
  {
    if (!ret.empty())
      ret += ",";
    if (interval.startTime == interval.endTime)
      ret += hhmm(interval.startTime);
    else
      ret += hhmm(interval.startTime) + "/" + hhmm(interval.endTime) + "/" +
             std::to_string(interval.resolution.total_seconds() / 60);
  }
  return ret;
}

std::string split(const std::vector<int>& mins)
{
  return describe(make_intervals(minutes(mins)));
}

// WMS capabilities of the time dimension with multiple intervals enabled
std::string capabilities(const std::vector<int>& mins)
{
  return make_time_dimension(minutes(mins))->getCapabilities(true, std::nullopt, std::nullopt);
}

}  // namespace

BOOST_AUTO_TEST_CASE(even_timesteps)
{
  BOOST_CHECK_EQUAL(split({0, 5, 10, 15, 20}), "00:00/00:20/5");
}

BOOST_AUTO_TEST_CASE(one_missing_time)
{
  BOOST_CHECK_EQUAL(split({0, 5, 10, 20, 25, 30}), "00:00/00:10/5,00:20/00:30/5");
}

BOOST_AUTO_TEST_CASE(two_missing_times)
{
  BOOST_CHECK_EQUAL(split({0, 5, 10, 25, 30, 35}), "00:00/00:10/5,00:25/00:35/5");
}

BOOST_AUTO_TEST_CASE(lone_time_between_gaps)
{
  BOOST_CHECK_EQUAL(split({0, 5, 10, 20, 30, 35, 40}), "00:00/00:10/5,00:20,00:30/00:40/5");
}

BOOST_AUTO_TEST_CASE(lone_time_between_gaps_before_two_time_run)
{
  // The test producer radar_gaps: frames 10:15 and 10:25 missing
  BOOST_CHECK_EQUAL(split({0, 5, 10, 20, 30, 35}), "00:00/00:10/5,00:20,00:30/00:35/5");
}

BOOST_AUTO_TEST_CASE(two_time_run_between_gaps)
{
  BOOST_CHECK_EQUAL(split({0, 5, 10, 20, 25, 35, 40, 45}),
                    "00:00/00:10/5,00:20/00:25/5,00:35/00:45/5");
}

BOOST_AUTO_TEST_CASE(lone_time_after_gap_at_end)
{
  // The newest radar frame arrives after a missing one
  BOOST_CHECK_EQUAL(split({0, 5, 10, 15, 25}), "00:00/00:15/5,00:25");
}

BOOST_AUTO_TEST_CASE(lone_time_before_gap_at_start)
{
  BOOST_CHECK_EQUAL(split({0, 10, 15, 20}), "00:00,00:10/00:20/5");
}

BOOST_AUTO_TEST_CASE(changing_timestep)
{
  // ECMWF style 1h -> 3h -> 6h timesteps
  std::vector<int> mins;
  for (int h = 0; h <= 6; h++)
    mins.push_back(60 * h);
  for (int h = 9; h <= 18; h += 3)
    mins.push_back(60 * h);
  for (int h = 24; h <= 48; h += 6)
    mins.push_back(60 * h);
  BOOST_CHECK_EQUAL(split(mins), "00:00/06:00/60,09:00/18:00/180,00:00/00:00/360");

  auto intervals = make_intervals(minutes(mins));
  BOOST_REQUIRE_EQUAL(intervals.size(), 3);
  BOOST_CHECK_EQUAL(intervals[2].startTime, base + Fmi::Hours(24));
  BOOST_CHECK_EQUAL(intervals[2].endTime, base + Fmi::Hours(48));
}

BOOST_AUTO_TEST_CASE(timestep_change_with_short_tail)
{
  // Formerly the whole time dimension degraded into a list of times
  BOOST_CHECK_EQUAL(split({0, 60, 120, 180, 360, 540}), "00:00/03:00/60,06:00/09:00/180");
}

BOOST_AUTO_TEST_CASE(irregular_times_give_no_intervals)
{
  BOOST_CHECK(make_intervals(minutes({0, 1, 3, 6, 10, 15})).empty());
  BOOST_CHECK(make_intervals(minutes({0, 5})).empty());
  BOOST_CHECK(make_intervals(minutes({0})).empty());
  BOOST_CHECK(make_intervals({}).empty());
}

BOOST_AUTO_TEST_CASE(unsorted_and_duplicate_times)
{
  BOOST_CHECK_EQUAL(split({20, 0, 10, 5, 10, 30, 25}), "00:00/00:10/5,00:20/00:30/5");
}

BOOST_AUTO_TEST_CASE(capabilities_with_gaps)
{
  BOOST_CHECK_EQUAL(capabilities({0, 5, 10, 20, 30, 35, 40}),
                    "2026-11-09T00:00:00Z/2026-11-09T00:10:00Z/PT5M,"
                    "2026-11-09T00:20:00Z,"
                    "2026-11-09T00:30:00Z/2026-11-09T00:40:00Z/PT5M");
}

BOOST_AUTO_TEST_CASE(capabilities_without_multiple_intervals)
{
  // When multiple intervals are disabled the individual times are listed
  auto td = make_time_dimension(minutes({0, 5, 10, 20, 30, 35}));
  BOOST_CHECK_EQUAL(td->getCapabilities(false, std::nullopt, std::nullopt),
                    "2026-11-09T00:00:00Z,2026-11-09T00:05:00Z,2026-11-09T00:10:00Z,"
                    "2026-11-09T00:20:00Z,2026-11-09T00:30:00Z,2026-11-09T00:35:00Z");
}

BOOST_AUTO_TEST_CASE(capabilities_for_requested_period)
{
  auto td = make_time_dimension(minutes({0, 5, 10, 20, 30, 35, 40}));

  // The lone time and the start of the last interval
  BOOST_CHECK_EQUAL(td->getCapabilities(true, base + Fmi::Minutes(20), base + Fmi::Minutes(35)),
                    "2026-11-09T00:20:00Z,"
                    "2026-11-09T00:30:00Z/2026-11-09T00:35:00Z/PT5M");

  // Only the lone time
  BOOST_CHECK_EQUAL(td->getCapabilities(true, base + Fmi::Minutes(15), base + Fmi::Minutes(25)),
                    "2026-11-09T00:20:00Z");

  // A period touching only the end of an interval
  BOOST_CHECK_EQUAL(td->getCapabilities(true, base + Fmi::Minutes(10), base + Fmi::Minutes(12)),
                    "2026-11-09T00:10:00Z");

  // Nothing in a gap
  BOOST_CHECK_EQUAL(td->getCapabilities(true, base + Fmi::Minutes(11), base + Fmi::Minutes(19)),
                    "");
}

BOOST_AUTO_TEST_CASE(timedimension_type)
{
  BOOST_CHECK(dynamic_cast<IntervalTimeDimension*>(
                  make_time_dimension(minutes({0, 5, 10, 20, 30})).get()) != nullptr);
  BOOST_CHECK(dynamic_cast<StepTimeDimension*>(make_time_dimension(minutes({0, 1, 3, 6})).get()) !=
              nullptr);
}

BOOST_AUTO_TEST_CASE(first_and_last_time)
{
  auto td = make_time_dimension(minutes({0, 5, 10, 15, 25}));
  BOOST_CHECK_EQUAL(td->firstTime(), base);
  BOOST_CHECK_EQUAL(td->lastTime(), base + Fmi::Minutes(25));
}

BOOST_AUTO_TEST_CASE(most_current_time_in_gap)
{
  // The wall clock is in the gap between t0+20 (about 20 minutes ago) and t0+70 (about 30 minutes
  // ahead): the closer one is chosen instead of the end of the last interval
  auto now = Fmi::SecondClock::universal_time();
  auto t0 = Fmi::DateTime(now.date(), Fmi::Minutes(now.time_of_day().total_seconds() / 60)) -
            Fmi::Minutes(40);
  std::vector<Fmi::DateTime> times;
  for (int m : {0, 5, 10, 15, 20, 70, 75, 80})
    times.push_back(t0 + Fmi::Minutes(m));
  BOOST_CHECK_EQUAL(make_time_dimension(times)->mostCurrentTime(), t0 + Fmi::Minutes(20));
}
