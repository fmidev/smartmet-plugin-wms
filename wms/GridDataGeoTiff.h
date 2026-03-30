// ======================================================================
/*!
 * \brief Shared utility for generating GeoTiff output from grid data.
 *
 * Any layer that queries the grid engine can produce a GeoTiff by calling
 * gridDataGeoTiff() with its own layer reference and the desired parameter
 * name. The function reuses all of the layer's paraminfo, projection,
 * unit conversion, and time settings.
 */
// ======================================================================

#pragma once
#include <string>

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
 * @param parameterName The parameter to query (e.g. "temperature", "windspeed").
 * @param interpolation Grid area interpolation method: "linear" or "nearest".
 * @param state         Current request state.
 * @return Raw GeoTiff bytes.
 * @throws Fmi::Exception on any error.
 */
std::string gridDataGeoTiff(Layer& layer,
                             const std::string& parameterName,
                             const std::string& interpolation,
                             State& state);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
