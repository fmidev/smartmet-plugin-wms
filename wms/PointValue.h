// ======================================================================
/*!
 * \brief Class to prioritize points with value
 */
// ======================================================================

#pragma once

#include "Positions.h"
#include <spine/Json.h>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct PointValueBase
{
  PointValueBase(const Positions::Point& p) : point(p) {}
  virtual ~PointValueBase() = default;
  virtual double priorityValue() const = 0;
  PointValueBase(const PointValueBase& other) = default;
  PointValueBase(PointValueBase&& other) = default;
  PointValueBase& operator=(const PointValueBase& other) = default;
  PointValueBase& operator=(PointValueBase&& other) = default;
  Positions::Point point;
};

struct PointValue : public PointValueBase
{
  PointValue(const Positions::Point& p, double val) : PointValueBase(p), value(val) {}
  double value;
  double priorityValue() const override { return value; }
};

struct WindArrowValue : public PointValueBase
{
  WindArrowValue(const Positions::Point& p, double d, double s)
      : PointValueBase(p), direction(d), speed(s)
  {
  }
  double direction;
  double speed;
  double priorityValue() const override { return speed; }
};

struct PointValueOptions
{
  void init(Json::Value& theJson);
  boost::optional<double> mindistance;
  boost::optional<std::string> priority;
  boost::optional<std::vector<int>> priorities;
};

std::vector<PointValue> prioritize(const std::vector<PointValue>& pv,
                                   const PointValueOptions& opts);
std::vector<WindArrowValue> prioritize(const std::vector<WindArrowValue>& pv,
                                       const PointValueOptions& opts);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
