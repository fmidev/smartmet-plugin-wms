// ======================================================================
/*!
 * \brief Various filters for isolines/isobands to smoothen them etc
 */
// ======================================================================

#pragma once

#include <gis/GeometrySmoother.h>
#include <gis/Types.h>
#include <json/json.h>
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

class IsolineFilter
{
 public:
  void init(Json::Value& theJson);
  std::size_t hash_value() const;

  void bbox(const Fmi::Box& box);
  void apply(std::vector<OGRGeometryPtr>& geoms, bool preserve_topology) const;

 private:
  Fmi::GeometrySmoother smoother;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
