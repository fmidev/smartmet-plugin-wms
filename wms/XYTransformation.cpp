#include "XYTransformation.h"
#include <gis/SpatialReference.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
XYTransformation::XYTransformation(const Fmi::SpatialReference& srs, const Projection& projection)
    : transformation(srs, projection.getCRS()), box(projection.getBox())
{
}

bool XYTransformation::transform(double longitude, double latitude, double& x, double& y)
{
  if (!transformation.transform(longitude, latitude))
    return false;

  box.transform(longitude, latitude);
  x = longitude;
  y = latitude;

  return true;
}

bool XYTransformation::transform(double& inoutX, double& inoutY)
{
  return transform(inoutX, inoutY, inoutX, inoutY);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
