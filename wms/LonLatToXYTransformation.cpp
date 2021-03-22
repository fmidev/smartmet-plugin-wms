#include "LonLatToXYTransformation.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <gis/SpatialReference.h>
#include <ogr_spatialref.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
LonLatToXYTransformation::LonLatToXYTransformation(const Projection& projection)
    : transformation(Fmi::SpatialReference("WGS84"), projection.getCRS()), box(projection.getBox())
{
}

bool LonLatToXYTransformation::transform(double longitude, double latitude, double& x, double& y)
{
  if (!transformation.transform(longitude, latitude))
    return false;

  box.transform(longitude, latitude);
  x = longitude;
  y = latitude;

  return true;
}

bool LonLatToXYTransformation::transform(double& inoutX, double& inoutY)
{
  return transform(inoutX, inoutY, inoutX, inoutY);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
