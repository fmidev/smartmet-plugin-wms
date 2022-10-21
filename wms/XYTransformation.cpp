#include "XYTransformation.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
XYTransformation::XYTransformation(const Projection& projection) : box(projection.getBox()) {}

void XYTransformation::transform(double longitude, double latitude, double& x, double& y)
{
  box.transform(longitude, latitude);
  x = longitude;
  y = latitude;
}

void XYTransformation::transform(double& inoutX, double& inoutY)
{
  box.transform(inoutX, inoutY);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
