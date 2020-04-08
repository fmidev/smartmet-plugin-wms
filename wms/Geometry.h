// ======================================================================
/*!
 * \brief gemetry utilities
 */
// ======================================================================

#pragma once

#include <boost/shared_ptr.hpp>
#include <gis/OGR.h>
#include <string>

namespace Fmi
{
class SpatialReference;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Geometry
{
std::string name(const OGRGeometry& theGeom, const std::string& theType);

std::string toString(const OGRGeometry& theGeom,
                     const std::string& theType,
                     const Fmi::Box& theBox,
                     const Fmi::SpatialReference& theSRS,
                     double thePrecision);

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
