#include "LonLatToXYTransformation.h"
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

void LonLatToXYTransformation::transform(double longitude, double latitude, double& x, double& y)
{
  transformation.Transform(longitude, latitude);
  box.transform(longitude, latitude);
  x = longitude;
  y = latitude;
}

void LonLatToXYTransformation::transform(double& inoutX, double& inoutY)
{
  transform(inoutX, inoutY, inoutX, inoutY);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
