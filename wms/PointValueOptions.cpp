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
  // Insert points to a map where points are grouped according to priority
  std::set<int> priority_categories;
  std::map<unsigned int, std::vector<PointValueWrapper>> prioritized_points;
  for (auto prio_category : priorities)
  {
    priority_categories.insert(prio_category);
    prioritized_points.insert(std::make_pair(prio_category, std::vector<PointValueWrapper>()));
  }

  // Non-prioritized points are inserted here
  auto non_prioritized_category = std::numeric_limits<int>::max();
  prioritized_points.insert(
      std::make_pair(non_prioritized_category, std::vector<PointValueWrapper>()));

  for (auto& item : points)
  {
    // Store as integer to get the right category
    auto prio_category = item.priorityValue();
    auto map_index =
        (priority_categories.count(prio_category) > 0 ? prio_category : non_prioritized_category);
    prioritized_points[map_index].push_back(item);
  }
  points.clear();

  // First insert prioritized points
  for (auto prio_category : priorities)
  {
    auto& prio_points = prioritized_points.at(prio_category);
    if (!prio_points.empty())
    {
      // Sort prioritized points in descending order
      std::sort(prio_points.begin(), prio_points.end(), max_sort);
      points.insert(points.end(), prio_points.begin(), prio_points.end());
    }
  }
  // Then Non-priorized points
  auto& prio_points = prioritized_points.at(non_prioritized_category);
  if (!prio_points.empty())
    points.insert(points.end(), prio_points.begin(), prio_points.end());
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