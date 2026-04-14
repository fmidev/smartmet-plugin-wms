// ======================================================================
/*!
 * \brief Utilities for generating data-encoded PNG tiles.
 *
 * A datatile is a standard PNG where RGBA pixel values encode raw float
 * data rather than visual colours.  This is the standard technique used
 * by web weather visualisation platforms (Windy, Mapbox, etc.) to deliver
 * gridded meteorological fields to browser-side JavaScript that renders
 * particle animations, interpolated overlays, and similar effects.
 *
 * Encoding schemes
 * ~~~~~~~~~~~~~~~~
 * Single band (one scalar parameter):
 *   R = high byte of 16-bit quantised value
 *   G = low  byte of 16-bit quantised value
 *   B = 0
 *   A = 255 for valid data, 0 for missing / nodata
 *
 *   Decode:  value = (R * 256 + G) / 65535.0 * (max - min) + min
 *
 * Dual band (two parameters, e.g. wind U + V):
 *   R = high byte of 16-bit quantised band-1 value
 *   G = low  byte of 16-bit quantised band-1 value
 *   B = high byte of 16-bit quantised band-2 value
 *   A = low  byte of 16-bit quantised band-2 value
 *
 *   Missing values: all four bytes are zero.  To distinguish from a
 *   genuine (min, min) value, the quantisation range maps [min, max]
 *   to [1, 65535], reserving 0 as the nodata sentinel.
 *
 *   Decode:  value1 = (R * 256 + G - 1) / 65534.0 * (max1 - min1) + min1
 *            value2 = (B * 256 + A - 1) / 65534.0 * (max2 - min2) + min2
 *
 * Scale / offset metadata is embedded in PNG tEXt chunks so that clients
 * can decode values without out-of-band information.
 */
// ======================================================================

#pragma once

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

// Write a single-band datatile PNG.
std::string writeSingleBandDataTile(int width,
                                     int height,
                                     const std::vector<float>& values);

// Write a dual-band datatile PNG (e.g. wind U + V).
std::string writeDualBandDataTile(int width,
                                   int height,
                                   const std::vector<float>& values1,
                                   const std::vector<float>& values2);

// Query grid engine for a single scalar parameter and return datatile PNG
// bytes.  Mirrors gridDataGeoTiff() from GridDataGeoTiff.h.
std::string gridDataTile(Layer& layer,
                          const std::string& parameterName,
                          const std::string& interpolation,
                          State& state);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
