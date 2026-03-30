// ======================================================================
/*!
 * \brief Shared utilities for generating GeoTiff output from grid data.
 *
 * gridDataGeoTiff()      — single-parameter query → single-band GeoTiff.
 * writeGeoTiffBands()    — write pre-computed float arrays as a multi-band GeoTiff.
 */
// ======================================================================

#pragma once
#include "Projection.h"
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class Layer;
class State;

/**
 * Query the grid engine for a single scalar parameter and write the result
 * as a deflate-compressed Float32 GeoTiff.
 *
 * @param layer         The layer whose paraminfo, projection, multiplier, offset,
 *                      origintime and valid time are used to build the query.
 * @param parameterName The parameter to query (e.g. "temperature", "WindSpeedMS").
 * @param interpolation Grid area interpolation method: "linear" or "nearest".
 * @param state         Current request state.
 * @return Raw GeoTiff bytes.
 * @throws Fmi::Exception on any error.
 */
std::string gridDataGeoTiff(Layer& layer,
                             const std::string& parameterName,
                             const std::string& interpolation,
                             State& state);

/**
 * Write pre-computed float arrays as a deflate-compressed multi-band Float32 GeoTiff.
 *
 * Each element of `bands` becomes one GeoTiff band (band 1 = bands[0], etc.).
 * All bands must have length == projection.xsize * projection.ysize and must
 * already be in north-up row order.
 *
 * @param projection  Must have xsize and ysize set; provides the geotransform box.
 * @param wkt         Coordinate system as WKT string.
 * @param bands       One float vector per band.
 * @return Raw GeoTiff bytes.
 * @throws Fmi::Exception on any error.
 */
std::string writeGeoTiffBands(const Projection& projection,
                               const std::string& wkt,
                               const std::vector<std::vector<float>>& bands);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
