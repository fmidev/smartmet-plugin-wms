#pragma once

#include "Projection.h"
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class State;

class XYTransformation
{
 public:
  XYTransformation(const Fmi::SpatialReference& srs, const Projection& projection);

  bool transform(double longitude, double latitude, double& x, double& y);
  bool transform(double& inoutX, double& inoutY);

 private:
  Fmi::CoordinateTransformation transformation;
  const Fmi::Box& box;
};
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
