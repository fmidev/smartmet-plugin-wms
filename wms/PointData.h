#pragma once

#include "Positions.h"
#include <macgyver/DateTime.h>
#include <optional>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class PointData
{
 public:
  PointData() = delete;
  explicit PointData(Positions::Point pt) : itsPoint(pt) {}
  void add(double value) { itsValues.push_back(value); }

  PointData(Positions::Point pt, double value) : itsPoint(pt), itsValues{value} {}

  PointData(Positions::Point pt, double val1, double val2) : itsPoint(pt), itsValues{val1, val2} {}

  bool empty() const { return itsValues.empty(); }
  std::size_t size() const { return itsValues.size(); }

  double& operator[](std::size_t index) { return itsValues[index]; }
  const double& operator[](std::size_t index) const { return itsValues[index]; }

  const Positions::Point& point() const { return itsPoint; }

  double priorityValue() const { return itsValues[0]; }

  // Observation time, currently set for flash and mobile observations only
  void time(const Fmi::DateTime& theTime) { itsTime = theTime; }
  const std::optional<Fmi::DateTime>& time() const { return itsTime; }

 private:
  Positions::Point itsPoint;
  std::vector<double> itsValues;
  std::optional<Fmi::DateTime> itsTime;
};
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
