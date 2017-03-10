// ======================================================================
/*!
 * \brief gemetry utilities
 */
// ======================================================================

#pragma once

#include <gis/OGR.h>
#include <boost/shared_ptr.hpp>
#include <string>

class OGRSpatialReference;

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
                     boost::shared_ptr<OGRSpatialReference> theSRS);

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
