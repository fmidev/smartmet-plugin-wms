#include "LonLatToXYTransformation.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <engines/gis/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
LonLatToXYTransformation::LonLatToXYTransformation(const Projection& projection,
                                                   const State& theState)
    : box(projection.getBox())
{
  auto sr = projection.getCRS();

  auto geocrs = theState.getGisEngine().getSpatialReference("WGS84");

  // Create the coordinate transformation from geonames coordinates to image coordinates
  transformation.reset(OGRCreateCoordinateTransformation(geocrs.get(), sr.get()));
  if (transformation == nullptr)
    throw std::runtime_error("Failed to create the needed coordinate transformation");
}

void LonLatToXYTransformation::transform(double longitude, double latitude, double& x, double& y)
{
  transformation->Transform(1, &longitude, &latitude);
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
