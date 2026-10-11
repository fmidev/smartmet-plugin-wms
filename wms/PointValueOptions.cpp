#include "PointValueOptions.h"
#include "Hash.h"
#include "JsonTools.h"
#include "PointData.h"
#include <macgyver/Exception.h>
#include <macgyver/NearTree.h>
#include <newbase/NFmiGlobals.h>
#include <algorithm>
#include <map>
#include <unordered_map>

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
  PointValueWrapper(PointData pv) : itsPointData(std::move(pv)) {}

  double x() const { return itsPointData.point().x; }
  double y() const { return itsPointData.point().y; }
  double priorityValue() const { return itsPointData.priorityValue(); }
  double priorityValueMeanDeviation() const { return itsMeanDeviation; }
  void setMeanDeviation(double mean)
  {
    itsMeanDeviation = abs(mean - itsPointData.priorityValue());
  }
  const PointData& data() const { return itsPointData; }

 private:
  double itsMeanDeviation = 0.0;
  PointData itsPointData;
};

bool max_sort(const PointValueWrapper& rval1, const PointValueWrapper& rval2)
{
  auto lhs = rval1.priorityValue();
  auto rhs = rval2.priorityValue();
  if (!std::isnan(lhs) && !std::isnan(rhs))
    return (rval1.priorityValue() > rval2.priorityValue());
  return !std::isnan(lhs);
}

bool min_sort(const PointValueWrapper& rval1, const PointValueWrapper& rval2)
{
  auto lhs = rval1.priorityValue();
  auto rhs = rval2.priorityValue();
  if (!std::isnan(lhs) && !std::isnan(rhs))
    return (rval1.priorityValue() < rval2.priorityValue());
  return !std::isnan(lhs);
}

bool mean_deviation_sort_max(const PointValueWrapper& rval1, const PointValueWrapper& rval2)
{
  auto lhs = rval1.priorityValueMeanDeviation();
  auto rhs = rval2.priorityValueMeanDeviation();
  if (!std::isnan(lhs) && !std::isnan(rhs))
    return (rval1.priorityValue() > rval2.priorityValue());
  return !std::isnan(lhs);
}

void sort_by_priority(std::vector<PointValueWrapper>& points, const std::string& priority)
{
  if (priority == "max")
    std::sort(points.begin(), points.end(), max_sort);
  else if (priority == "min")
    std::sort(points.begin(), points.end(), min_sort);
  else if (priority == "extrema")
  {
    // Calculate mean of non NaN values
    double sum = 0;
    int n = 0;
    for (const auto& item : points)
    {
      if (!std::isnan(item.priorityValue()))
      {
        sum += item.priorityValue();
        ++n;
      }
    }
    // sort only if there are non NaN values
    if (n > 0)
    {
      auto mean = sum / n;
      for (auto& item : points)
        item.setMeanDeviation(mean);
      std::sort(points.begin(), points.end(), mean_deviation_sort_max);
    }
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

// Like sort_by_priorities, but the priority list is selected separately for each point
void sort_by_conditional_priorities(std::vector<PointValueWrapper>& points,
                                    const std::vector<ConditionalPriorities>& tables,
                                    const std::vector<std::string>& extra_parameters,
                                    std::size_t first_extra_column)
{
  // symbol --> priority number for each table, zero for most important
  std::vector<std::unordered_map<int, int>> table_priorities;
  std::vector<std::optional<std::size_t>> table_columns;
  for (const auto& table : tables)
  {
    std::unordered_map<int, int> symbol_priorities;
    for (auto i = 0UL; i < table.priorities.size(); i++)
      symbol_priorities.insert({table.priorities[i], i});
    table_priorities.push_back(std::move(symbol_priorities));

    std::optional<std::size_t> column;
    if (table.parameter)
    {
      auto pos = std::find(extra_parameters.begin(), extra_parameters.end(), *table.parameter);
      column = first_extra_column + (pos - extra_parameters.begin());
    }
    table_columns.push_back(column);
  }

  auto no_priority = std::numeric_limits<int>::max();
  std::multimap<int, PointValueWrapper> prioritized_points;
  for (const auto& item : points)
  {
    auto priority = no_priority;
    const auto& data = item.data();

    // The first table accepting the point decides the priority
    for (auto i = 0UL; i < tables.size(); i++)
    {
      if (table_columns[i])
      {
        if (*table_columns[i] >= data.size())
          throw Fmi::Exception(BCP, "Priority parameter value not available")
              .addParameter("parameter", *tables[i].parameter);
        if (!tables[i].accepts(data[*table_columns[i]]))
          continue;
      }
      auto symbol = item.priorityValue();
      auto pos = table_priorities[i].find(symbol);
      if (pos != table_priorities[i].end())
        priority = pos->second;
      break;
    }
    prioritized_points.insert({priority, item});
  }

  points.clear();
  for (const auto& priority_item : prioritized_points)
    points.push_back(priority_item.second);
}

std::vector<int> parse_priority_list(const Json::Value& theJson)
{
  if (!theJson.isArray())
    throw Fmi::Exception(BCP, "Priority list must be an array of values");
  std::vector<int> ret;
  for (const auto& prio : theJson)
    ret.push_back(prio.asInt());
  return ret;
}

ConditionalPriorities parse_conditional_priorities(Json::Value& theJson)
{
  ConditionalPriorities ret;
  JsonTools::remove_string(ret.parameter, theJson, "parameter");
  JsonTools::remove_double(ret.lolimit, theJson, "lolimit");
  JsonTools::remove_double(ret.hilimit, theJson, "hilimit");
  auto json = JsonTools::remove(theJson, "priority");
  if (json.isNull())
    throw Fmi::Exception(BCP, "Conditional priority setting is missing the priority list");
  ret.priorities = parse_priority_list(json);

  if (!ret.parameter && (ret.lolimit || ret.hilimit))
    throw Fmi::Exception(BCP, "Priority limits given without a parameter");

  if (!theJson.empty())
    throw Fmi::Exception(BCP, "Unknown settings in conditional priority")
        .addParameter("settings", theJson.toStyledString());
  return ret;
}

}  // namespace

bool ConditionalPriorities::accepts(double theValue) const
{
  if (!parameter)
    return true;
  if (theValue == kFloatMissing || std::isnan(theValue))
    return false;
  if (lolimit && *lolimit > theValue)
    return false;
  if (hilimit && *hilimit < theValue)
    return false;
  return true;
}

std::size_t ConditionalPriorities::hash_value() const
{
  auto hash = Fmi::hash_value(parameter);
  Fmi::hash_combine(hash, Fmi::hash_value(lolimit));
  Fmi::hash_combine(hash, Fmi::hash_value(hilimit));
  for (auto symbol : priorities)
    Fmi::hash_combine(hash, Fmi::hash_value(symbol));
  return hash;
}

std::vector<std::string> PointValueOptions::extraParameters() const
{
  std::vector<std::string> ret;
  for (const auto& table : conditional_priorities)
    if (table.parameter && std::find(ret.begin(), ret.end(), *table.parameter) == ret.end())
      ret.push_back(*table.parameter);
  return ret;
}

void PointValueOptions::init(Json::Value& theJson)
{
  try
  {
    JsonTools::remove_double(mindistance, theJson, "mindistance");
    JsonTools::remove_string(rendering_order, theJson, "rendering_order");

    auto json = JsonTools::remove(theJson, "priority");
    if (!json.isNull())
    {
      if (json.isArray() && !json.empty() && json[0].isObject())
      {
        // Conditional priority lists
        for (auto& item : json)
        {
          if (!item.isObject())
            throw Fmi::Exception(BCP, "Conditional priority lists cannot be mixed with values");
          conditional_priorities.push_back(parse_conditional_priorities(item));
        }
      }
      else if (json.isArray())
        priorities = parse_priority_list(json);
      else
        priority = json.asString();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<PointData> prioritize(const std::vector<PointData>& pv,
                                  const PointValueOptions& opts,
                                  std::size_t first_extra_column)
{
  try
  {
    std::vector<PointValueWrapper> points;
    points.reserve(pv.size());
    for (const auto& item : pv)
      points.emplace_back(item);

    if (opts.priority)
      sort_by_priority(points, *opts.priority);
    else if (opts.priorities)
      sort_by_priorities(points, *opts.priorities);
    else if (!opts.conditional_priorities.empty())
      sort_by_conditional_priorities(
          points, opts.conditional_priorities, opts.extraParameters(), first_extra_column);

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

    if (opts.rendering_order == "normal")
      ;
    else if (opts.rendering_order == "reverse")
      std::reverse(ret.begin(), ret.end());
    else
      throw Fmi::Exception(BCP, "Unknown rendering order '" + opts.rendering_order + "'");

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t PointValueOptions::hash_value() const
{
  try
  {
    auto hash = Fmi::hash_value(mindistance);
    Fmi::hash_combine(hash, Fmi::hash_value(priority));
    if (priorities)
    {
      for (auto symbol : *priorities)
        Fmi::hash_combine(hash, Fmi::hash_value(symbol));
    }
    for (const auto& table : conditional_priorities)
      Fmi::hash_combine(hash, table.hash_value());
    Fmi::hash_combine(hash, Fmi::hash_value(rendering_order));

    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to calculate hash value for point value priorities!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
