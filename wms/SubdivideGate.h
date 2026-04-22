#pragma once

#include <gis/Box.h>
#include <gis/CoordinateMatrix.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// Decide the effective subdivide value for bilinear cell densification.
//
// Returns the requested value when the projected data cells are comfortably
// larger than an output pixel; otherwise returns 0 so Trax skips densification
// it would do at sub-pixel resolution where the extra samples cannot possibly
// be visible. The cell size is estimated from a 5x5 uniform sub-lattice sample
// of the projected coordinate matrix and summarised by an approximate median
// (std::nth_element), which is within a few nanoseconds on the WMS hot path.
//
// For highly distorted geographic projections the cell size varies with
// latitude; a 5x5 sample picks up that variation well enough that the single
// global decision (either on or off) matches what a conservative per-cell
// decision would do for the majority of the image. Extreme cases (pole-to-
// pole views, etc.) are self-gating: the zoom level that makes cells tiny is
// the same zoom level where densify would be invisible.
inline int effective_subdivide(int requested,
                               double min_cell_pixels,
                               const Fmi::CoordinateMatrix& coords,
                               const Fmi::Box& box)
{
  if (requested <= 0)
    return requested;
  if (min_cell_pixels <= 0.0)
    return requested;

  const auto nx = coords.width();
  const auto ny = coords.height();
  if (nx < 2 || ny < 2)
    return 0;

  const double pxw = static_cast<double>(box.width());
  const double pxh = static_cast<double>(box.height());
  if (!(pxw > 0.0) || !(pxh > 0.0))
    return requested;

  const double pixel_dx = (box.xmax() - box.xmin()) / pxw;
  const double pixel_dy = (box.ymax() - box.ymin()) / pxh;
  const double pixel_size = std::min(std::abs(pixel_dx), std::abs(pixel_dy));
  if (!(pixel_size > 0.0))
    return requested;

  constexpr std::size_t S = 5;
  std::array<double, S * S> samples{};
  std::size_t idx = 0;

  for (std::size_t sj = 0; sj < S; ++sj)
  {
    const auto j = (sj + 1) * (ny - 1) / (S + 1);
    if (j + 1 >= ny)
      continue;
    for (std::size_t si = 0; si < S; ++si)
    {
      const auto i = (si + 1) * (nx - 1) / (S + 1);
      if (i + 1 >= nx)
        continue;

      const auto p00 = coords(i, j);
      const auto p10 = coords(i + 1, j);
      const auto p01 = coords(i, j + 1);

      const double dx = std::hypot(p10.first - p00.first, p10.second - p00.second);
      const double dy = std::hypot(p01.first - p00.first, p01.second - p00.second);
      if (!std::isfinite(dx) || !std::isfinite(dy))
        continue;

      samples[idx++] = std::min(dx, dy);
    }
  }

  if (idx == 0)
    return 0;

  const auto mid = samples.begin() + idx / 2;
  std::nth_element(samples.begin(), mid, samples.begin() + idx);
  const double median_cell = *mid;

  return (median_cell < min_cell_pixels * pixel_size) ? 0 : requested;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
