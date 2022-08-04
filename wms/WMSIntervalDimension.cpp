#include "WMSIntervalDimension.h"
#include <macgyver/StringConversion.h>
#include <iostream>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
WMSIntervalDimension::WMSIntervalDimension(int interval_start,
                                           int interval_end,
                                           bool interval_default)
{
  itsIntervals.push_back(interval_dimension_item(interval_start, interval_end, interval_default));
}

void WMSIntervalDimension::addInterval(int interval_start, int interval_end, bool interval_default)
{
  itsIntervals.push_back(interval_dimension_item(interval_start, interval_end, interval_default));
}

std::string WMSIntervalDimension::getStartIntervals() const
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

std::string WMSIntervalDimension::getEndIntervals() const
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

std::string WMSIntervalDimension::getIntervals() const
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

std::string WMSIntervalDimension::getDefaultStartInterval() const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_default)
      return Fmi::to_string(item.interval_start);
  }

  // If no default set, the first is defauilt
  if (!itsIntervals.empty())
  {
    const auto& first_item = itsIntervals.front();
    return Fmi::to_string(first_item.interval_start);
  }

  return "";
}

std::string WMSIntervalDimension::getDefaultEndInterval() const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_default)
      return Fmi::to_string(item.interval_end);
  }

  // If no default set, the first is defauilt
  if (!itsIntervals.empty())
  {
    const auto& first_item = itsIntervals.front();
    return Fmi::to_string(first_item.interval_end);
  }

  return "";
}

std::string WMSIntervalDimension::getDefaultInterval() const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_default)
      return (Fmi::to_string(item.interval_start) + "," + Fmi::to_string(item.interval_end));
  }

  // If no default set, the first is defauilt
  if (!itsIntervals.empty())
  {
    const auto& first_item = itsIntervals.front();
    return (Fmi::to_string(first_item.interval_start) + "," +
            Fmi::to_string(first_item.interval_end));
  }

  return "";
}

bool WMSIntervalDimension::isValidInterval(int interval_start, int interval_end) const
{
  for (const auto& item : itsIntervals)
  {
    if (item.interval_start == interval_start && item.interval_end == interval_end)
      return true;
  }

  return false;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
