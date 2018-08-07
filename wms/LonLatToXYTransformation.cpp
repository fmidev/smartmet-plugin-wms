#include "LonLatToXYTransformation.h"
#include <boost/move/make_unique.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
LonLatToXYTransformation::LonLatToXYTransformation(const Projection& projection)
    : box(projection.getBox())
{
  boost::shared_ptr<OGRSpatialReference> sr = projection.getCRS();

  auto geocrs = boost::movelib::make_unique<OGRSpatialReference>();
  OGRErr err = geocrs->SetFromUserInput("WGS84");
  if (err != OGRERR_NONE)
    throw std::runtime_error("GDAL does not understand crs 'WGS84'");

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
