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
class State;

namespace Geometry
{
std::string name(const OGRGeometry& theGeom, const std::string& theType);

std::string toString(const OGRGeometry& theGeom,
                     const std::string& theType,
                     const Fmi::Box& theBox,
                     const Fmi::SpatialReference& theSRS,
                     double thePrecision);

std::string toString(const OGRGeometry& theGeom,
                     const std::string& theType,
                     const Fmi::Box& theBox,
                     const Fmi::SpatialReference& theSRS,
                     double thePrecision,
                     std::map<std::size_t,uint>& arcHashMap,
                     uint& arcCounter,
                     std::string& arcsNumbers,
                     std::string& arcsCoordinates);

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
