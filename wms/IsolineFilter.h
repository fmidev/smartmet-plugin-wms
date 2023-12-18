// ======================================================================
/*!
 * \brief Various filters for isolines/isobands to smoothen them etc
 */
// ======================================================================

#pragma once

#include <gis/Types.h>
#include <json/json.h>
#include <newbase/NFmiInfoData.h>
#include <cairo.h>
#include <vector>

namespace Fmi
{
class Box;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class IsolineFilter
{
 public:
  enum class Type
  {
    None,      // disabled
    Average,   // moving average, weight = 1
    Linear,    // moving average, weight = 1/(1+distance)
    Gaussian,  // moving average, weight = Gaussian where stdev is selected based on radius
    Tukey      // moving average, weight = Tukey's biweight = (1-(distance/radius)^2)^2
  };

  enum class Metric
  {
    Euclidian,  // 2D distance
    Path        // 2D distance along the path
  };

  void init(Json::Value& theJson);
  std::size_t hash_value() const;

  void bbox(const Fmi::Box& box);
  void apply(std::vector<OGRGeometryPtr>& geoms, bool preserve_topology) const;

 private:
  Type type = Type::None;             // no filtering done by default
  Metric metric = Metric::Euclidian;  // distance along path

  double radius = 0;    // in pixels
  uint iterations = 1;  // one pass only by default
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
