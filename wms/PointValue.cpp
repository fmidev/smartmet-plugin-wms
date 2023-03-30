#include "PointValue.h"
#include "JsonTools.h"
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
  PointValueWrapper(const PointValueBase* pvb) : itsPointValueBase(pvb) {}

  double x() const { return itsPointValueBase->point.x; }
  double y() const { return itsPointValueBase->point.y; }
  double priorityValue() const { return itsPointValueBase->priorityValue(); }
  double priorityValueMeanDeviation() const { return itsMeanDeviation; }
  void setMeanDeviation(double mean)
  {
    itsMeanDeviation = abs(mean - itsPointValueBase->priorityValue());
  }
  const PointValueBase& data() const { return *itsPointValueBase; };

 private:
  double itsMeanDeviation{0};
  const PointValueBase* itsPointValueBase;
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

template <typename T>
std::vector<T> prioritize_t(const std::vector<T>& pv, const PointValueOptions& opts)
{
  try
  {
    std::vector<T> ret;

    std::vector<PointValueWrapper> points;
    points.reserve(pv.size());
    for (const auto& item : pv)
      points.push_back(&item);

    if (opts.priority)
    {
      if (*opts.priority == "max")
        std::sort(points.begin(), points.end(), max_sort);
      else if (*opts.priority == "min")
        std::sort(points.begin(), points.end(), min_sort);
      else if (*opts.priority == "extrema")
      {
        double sum = 0;
        for (const auto& item : points)
          sum += item.priorityValue();
        double mean = (sum / points.size());
        for (auto& item : points)
          item.setMeanDeviation(mean);
        std::sort(points.begin(), points.end(), mean_deviation_sort_max);
      }
      else if (*opts.priority != "none")
      {
        throw Fmi::Exception::Trace(BCP, "Invalid priority-option: " + *opts.priority);
      }
    }
    else if (opts.priorities)
    {
      // Insert points to a map where points are grouped according to priority
      std::set<int> priority_categories;
      std::map<unsigned int, std::vector<PointValueWrapper>> prioritized_points;
      for (auto prio_category : *opts.priorities)
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
        auto map_index = (priority_categories.count(prio_category) > 0 ? prio_category
                                                                       : non_prioritized_category);
        prioritized_points[map_index].push_back(item);
      }
      points.clear();
      // First insert prioritized points
      for (auto prio_category : *opts.priorities)
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

    // Weed out points if mindistance  > 0
    auto mindistance = (opts.mindistance ? *opts.mindistance : 0.0);
    if (mindistance > 0)
    {
      Fmi::NearTree<PointValueWrapper> accepted_points;
      for (auto& p : points)
      {
        // Skip the point if it is too close to some other point already accepted
        auto match = accepted_points.nearest(p, mindistance);
        if (match)
          continue;
        accepted_points.insert(p);
        ret.push_back(dynamic_cast<const T&>(p.data()));
      }
    }
    else
    {
      for (const auto& p : points)
        ret.push_back(dynamic_cast<const T&>(p.data()));
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
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

std::vector<PointValue> prioritize(const std::vector<PointValue>& pv, const PointValueOptions& opts)
{
  return prioritize_t(pv, opts);
}

std::vector<WindArrowValue> prioritize(const std::vector<WindArrowValue>& pv,
                                       const PointValueOptions& opts)
{
  return prioritize_t(pv, opts);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
