// ======================================================================
/*!
 * \brief GeoTiff output layer
 *
 * Produces raw GeoTiff output by fetching grid data directly from the
 * grid engine and writing it via GDAL. The normal CTPP/SVG/Cairo
 * rendering pipeline is bypassed entirely.
 *
 * Uses the same parameter/producer/level/projection JSON structure as
 * the raster layer — just replace layer_type "raster" with "geotiff".
 * Only grid-engine data sources are supported (not point observations).
 */
// ======================================================================

#pragma once

#include "RasterLayer.h"
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class State;

class GeoTiffLayer : public RasterLayer
{
 public:
  // generate() is intentionally a no-op: geotiff products bypass the CTPP pipeline
  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  // Core entry point: fetches grid data and returns GeoTiff bytes
  std::string generateGeoTiff(State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
