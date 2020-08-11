// ======================================================================
/*!
 * \brief gemetry utilities
 */
// ======================================================================

#pragma once

#include <boost/shared_ptr.hpp>
#include <gis/OGR.h>
#include <string>

class OGRSpatialReference;

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class State;

namespace Geometry
{
std::string name(const OGRGeometry& theGeom, const State& theState);

std::string toString(const OGRGeometry& theGeom,
                     const State& theState,
                     const Fmi::Box& theBox,
                     const std::shared_ptr<OGRSpatialReference>& theSRS,
                     double thePrecision);

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
