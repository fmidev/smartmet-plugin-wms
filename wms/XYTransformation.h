#pragma once

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
