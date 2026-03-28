#include "IntervalDimension.h"
#include <macgyver/StringConversion.h>
#include <iostream>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
IntervalDimension::~IntervalDimension() = default;

IntervalDimension::IntervalDimension(int interval_start,
                                           int interval_end,
                                           bool interval_default)
{
  itsIntervals.emplace_back(interval_start, interval_end, interval_default);
}

void IntervalDimension::addInterval(int interval_start, int interval_end, bool interval_default)
{
  itsIntervals.emplace_back(interval_start, interval_end, interval_default);
}

std::string IntervalDimension::getStartIntervals() const
{
  std::string ret;

  std::set<int> interval_set;
  for (const auto& item : itsIntervals)
  {
    if (interval_set.find(item.interval_start) != interval_set.end())
      continue;
    if (!ret.empty())
      ret += ",";
    ret += Fmi::to_string(item.interval_start);
    interval_set.insert(item.interval_start);
  }

  return ret;
}

std::string IntervalDimension::getEndIntervals() const
{
  std::string ret;
  std::set<int> interval_set;
  for (const auto& item : itsIntervals)
  {
    if (interval_set.find(item.interval_end) != interval_set.end())
      continue;
    if (!ret.empty())
      ret += ",";
    ret += Fmi::to_string(item.interval_end);
    interval_set.insert(item.interval_end);
  }

  return ret;
}

std::string IntervalDimension::getIntervals() const
{
  std::string ret;

  for (const auto& item : itsIntervals)
  {
    if (!ret.empty())
      ret += " ";
    ret += (Fmi::to_string(item.interval_start) + "," + Fmi::to_string(item.interval_end));
  }

  return ret;
}

std::string IntervalDimension::getDefaultStartInterval() const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_default)
      return Fmi::to_string(item.interval_start);
  }

  // If no default set, the first is default
  if (!itsIntervals.empty())
  {
    const auto& first_item = itsIntervals.front();
    return Fmi::to_string(first_item.interval_start);
  }

  return "";
}

std::string IntervalDimension::getDefaultEndInterval() const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_default)
      return Fmi::to_string(item.interval_end);
  }

  // If no default set, the first is default
  if (!itsIntervals.empty())
  {
    const auto& first_item = itsIntervals.front();
    return Fmi::to_string(first_item.interval_end);
  }

  return "";
}

std::string IntervalDimension::getDefaultInterval() const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_default)
      return (Fmi::to_string(item.interval_start) + "," + Fmi::to_string(item.interval_end));
  }

  // If no default set, the first is default
  if (!itsIntervals.empty())
  {
    const auto& first_item = itsIntervals.front();
    return (Fmi::to_string(first_item.interval_start) + "," +
            Fmi::to_string(first_item.interval_end));
  }

  return "";
}

bool IntervalDimension::isValidInterval(int interval_start, int interval_end) const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_start == interval_start && item.interval_end == interval_end)
      return true;
  }

  return false;
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
