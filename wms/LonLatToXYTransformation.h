

#pragma once

#include "Geometry.h"
#include "Projection.h"
#include <gis/Box.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class LonLatToXYTransformation
{
 public:
  LonLatToXYTransformation(const Projection& projection);

  void transform(double longitude, double latitude, double& x, double& y);
  void transform(double& inoutX, double& inoutY);

 private:
  std::unique_ptr<OGRCoordinateTransformation> transformation;
  const Fmi::Box& box;
};
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
