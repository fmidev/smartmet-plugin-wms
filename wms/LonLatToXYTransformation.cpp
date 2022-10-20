#include "LonLatToXYTransformation.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
LonLatToXYTransformation::LonLatToXYTransformation(const Projection& projection)
    : transformation("WGS84", projection)
{
}

bool LonLatToXYTransformation::transform(double longitude, double latitude, double& x, double& y)
{
  return transformation.transform(longitude, latitude, x, y);
}

bool LonLatToXYTransformation::transform(double& inoutX, double& inoutY)
{
  return transformation.transform(inoutX, inoutY, inoutX, inoutY);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
