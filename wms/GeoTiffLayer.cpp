// ======================================================================

#include "GeoTiffLayer.h"
#include "State.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// ----------------------------------------------------------------------
/*!
 * \brief No-op generate — geotiff products bypass the CTPP pipeline
 */
// ----------------------------------------------------------------------
void GeoTiffLayer::generate(CTPP::CDT& /*theGlobals*/,
                             CTPP::CDT& /*theLayersCdt*/,
                             State& /*theState*/)
{
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
