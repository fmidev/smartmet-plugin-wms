#include "PointValueOptions.h"
#include "JsonTools.h"
#include "PointData.h"
#include <macgyver/Exception.h>
#include <macgyver/NearTree.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
class PointValueWrapper
{
 public:
  PointValueWrapper(const PointData& pv) : itsPointData(pv) {}

  double x() const { return itsPointData.point().x; }
  double y() const { return itsPointData.point().y; }
  double priorityValue() const { return itsPointData.priorityValue(); }
  double priorityValueMeanDeviation() const { return itsMeanDeviation; }
  void setMeanDeviation(double mean)
  {
    itsMeanDeviation = abs(mean - itsPointData.priorityValue());
  }
  const PointData& data() const { return itsPointData; };

 private:
  double itsMeanDeviation = 0.0;
  PointData itsPointData;
};

bool max_sort(const PointValueWrapper& rval1, const PointValueWrapper& rval2)
{
  return (rval1.priorityValue() > rval2.priorityValue());
}

bool min_sort(const PointValueWrapper& rval1, const PointValueWrapper& rval2)
{
  return (rval1.priorityValue() < rval2.priorityValue());
}

bool mean_deviation_sort_max(const PointValueWrapper& rval1, const PointValueWrapper& rval2)
{
  return (rval1.priorityValueMeanDeviation() > rval2.priorityValueMeanDeviation());
}

void sort_by_priority(std::vector<PointValueWrapper>& points, const std::string& priority)
{
  if (priority == "max")
    std::sort(points.begin(), points.end(), max_sort);
  else if (priority == "min")
    std::sort(points.begin(), points.end(), min_sort);
  else if (priority == "extrema")
  {
    double sum = 0;
    for (const auto& item : points)
      sum += item.priorityValue();
    double mean = (sum / points.size());
    for (auto& item : points)
      item.setMeanDeviation(mean);
    std::sort(points.begin(), points.end(), mean_deviation_sort_max);
  }
  else if (priority != "none")
  {
    throw Fmi::Exception::Trace(BCP, "Invalid priority-option: " + priority);
  }
}

void sort_by_priorities(std::vector<PointValueWrapper>& points, const std::vector<int>& priorities)
{
  // priorities contains an ordered list of symbol values which determine the drawing order.
  // any symbol not in the list is drawn last in no particular order

  std::unordered_map<int, int>
      symbol_priorities;  // symbol --> priority number, zero for most important
  for (auto i = 0UL; i < priorities.size(); i++)
    symbol_priorities.insert({priorities[i], i});

  // Then insert the points into a multimap for rendering

  auto no_priority = std::numeric_limits<int>::max();
  std::multimap<int, PointValueWrapper> prioritized_points;
  for (const auto& item : points)
  {
    auto symbol = item.priorityValue();
    auto pos = symbol_priorities.find(symbol);
    auto priority = (pos == symbol_priorities.end() ? no_priority : pos->second);
    prioritized_points.insert({priority, item});
  }

  // And insert back into points in sorted order
  points.clear();
  for (const auto& priority_item : prioritized_points)
    points.push_back(priority_item.second);
}

}  // namespace

void PointValueOptions::init(Json::Value& theJson)
{
  try
  {
    JsonTools::remove_double(mindistance, theJson, "mindistance");

    auto json = JsonTools::remove(theJson, "priority");
    if (!json.isNull())
    {
      if (json.isArray())
      {
        priorities = std::vector<int>();
        for (const auto& prio : json)
          priorities->push_back(prio.asInt());
      }
      else
        priority = json.asString();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<PointData> prioritize(const std::vector<PointData>& pv, const PointValueOptions& opts)
{
  try
  {
    std::vector<PointValueWrapper> points;
    points.reserve(pv.size());
    for (const auto& item : pv)
      points.push_back(item);

    if (opts.priority)
      sort_by_priority(points, *opts.priority);
    else if (opts.priorities)
      sort_by_priorities(points, *opts.priorities);

    std::vector<PointData> ret;

    // Weed out points if mindistance  > 0
    if (opts.mindistance > 0.0)
    {
      Fmi::NearTree<PointValueWrapper> accepted_points;
      for (auto& p : points)
      {
        // Skip the point if it is too close to some other point already accepted
        auto match = accepted_points.nearest(p, *opts.mindistance);
        if (match)
          continue;
        accepted_points.insert(p);
        ret.push_back(p.data());
      }
    }
    else
    {
      for (const auto& p : points)
        ret.push_back(p.data());
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
