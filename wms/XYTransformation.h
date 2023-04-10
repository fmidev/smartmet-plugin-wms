#pragma once

#include "Projection.h"
#include <gis/Box.h>

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
  explicit XYTransformation(const Projection& projection);

  void transform(double longitude, double latitude, double& x, double& y);
  void transform(double& inoutX, double& inoutY);

 private:
  const Fmi::Box& box;
};
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
