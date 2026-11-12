#pragma once

#include <gis/CoordinateTransformation.h>

namespace Fmi
{
class Box;
}  // namespace Fmi
namespace SmartMet::Plugin::Dali
{
class Projection;
}  // namespace SmartMet::Plugin::Dali

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class LonLatToXYTransformation
{
 public:
  explicit LonLatToXYTransformation(const Projection& projection);

  bool transform(double longitude, double latitude, double& x, double& y);
  bool transform(double& inoutX, double& inoutY);

 private:
  Fmi::CoordinateTransformation transformation;
  const Fmi::Box& box;
};
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
