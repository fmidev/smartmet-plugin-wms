// ======================================================================
// Unit tests for SubdivideGate::effective_subdivide().
//
// Verifies the sub-pixel gate that silently forces a layer's requested
// bilinear subdivision count to 0 whenever the projected data cells are
// too small relative to the output pixel size for interior samples to be
// visible. Header-only helper lives in wms/SubdivideGate.h.
//
// Run with: make test
// ======================================================================

#define BOOST_TEST_MODULE SubdivideGateTest
#include <boost/test/unit_test.hpp>

#include "SubdivideGate.h"

#include <cmath>
#include <limits>

using SmartMet::Plugin::Dali::effective_subdivide;

namespace
{
// Build a rectilinear CoordinateMatrix spanning [0..span] x [0..span] with nx*ny
// grid points. Cell size in world units is span / (nx - 1).
Fmi::CoordinateMatrix make_grid(std::size_t nx, std::size_t ny, double span)
{
  return Fmi::CoordinateMatrix(nx, ny, 0.0, 0.0, span, span);
}

// Build a Box spanning [0..span] x [0..span] mapped onto a width x height pixel
// viewport. Pixel size in world units is span / width (and span / height).
Fmi::Box make_box(double span, std::size_t width, std::size_t height)
{
  return Fmi::Box(0.0, 0.0, span, span, width, height);
}
}  // namespace

// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(requested_zero_is_returned_as_zero)
{
  auto coords = make_grid(20, 20, 100.0);
  auto box = make_box(100.0, 400, 400);

  BOOST_CHECK_EQUAL(effective_subdivide(0, 2.0, coords, box), 0);
}

BOOST_AUTO_TEST_CASE(negative_threshold_disables_the_gate)
{
  // Cell size / pixel size = 1.0 — well below the default threshold of 2.0
  // but the gate should be bypassed when the threshold is non-positive.
  auto coords = make_grid(101, 101, 100.0);  // cell = 1.0 world units
  auto box = make_box(100.0, 100, 100);      // pixel = 1.0 world units

  BOOST_CHECK_EQUAL(effective_subdivide(4, 0.0, coords, box), 4);
  BOOST_CHECK_EQUAL(effective_subdivide(4, -1.0, coords, box), 4);
}

BOOST_AUTO_TEST_CASE(degenerate_grid_returns_zero)
{
  Fmi::CoordinateMatrix one_by_one(1, 1);
  auto box = make_box(100.0, 400, 400);

  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, one_by_one, box), 0);
}

BOOST_AUTO_TEST_CASE(cells_much_larger_than_pixels_pass_through)
{
  // 10 world-unit cells vs 1 world-unit pixels → 10 pixels per cell.
  auto coords = make_grid(11, 11, 100.0);  // cell = 10.0
  auto box = make_box(100.0, 100, 100);    // pixel = 1.0

  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, box), 4);
  BOOST_CHECK_EQUAL(effective_subdivide(8, 2.0, coords, box), 8);
}

BOOST_AUTO_TEST_CASE(cells_much_smaller_than_pixels_are_gated)
{
  // 1 world-unit cells vs 5 world-unit pixels → 0.2 pixels per cell.
  auto coords = make_grid(101, 101, 100.0);  // cell = 1.0
  auto box = make_box(100.0, 20, 20);        // pixel = 5.0

  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, box), 0);
  BOOST_CHECK_EQUAL(effective_subdivide(10, 2.0, coords, box), 0);
}

BOOST_AUTO_TEST_CASE(cells_at_threshold_boundary)
{
  // Cells exactly 2 world units, pixels exactly 1 world unit → ratio 2.0.
  // The gate uses strict less-than, so ratio == threshold passes through.
  auto coords = make_grid(51, 51, 100.0);  // cell = 2.0
  auto box = make_box(100.0, 100, 100);    // pixel = 1.0

  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, box), 4);

  // Same cell size with a larger pixel_size pushes the ratio below threshold.
  auto box_coarser = make_box(100.0, 40, 40);  // pixel = 2.5 → ratio 0.8
  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, box_coarser), 0);
}

BOOST_AUTO_TEST_CASE(anisotropic_cells_use_the_shorter_dimension)
{
  // 2 world units wide, 0.5 world units tall (wider than tall).
  // Pixel 1x1. Shorter dimension is 0.5, ratio 0.5 < 2.0 → gated.
  Fmi::CoordinateMatrix coords(11, 41);
  for (std::size_t j = 0; j < 41; ++j)
    for (std::size_t i = 0; i < 11; ++i)
      coords.set(i, j, 2.0 * static_cast<double>(i), 0.5 * static_cast<double>(j));
  auto box = make_box(20.0, 20, 20);  // pixel = 1.0

  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, box), 0);
}

BOOST_AUTO_TEST_CASE(nan_cells_are_skipped_but_valid_ones_still_decide)
{
  // Mostly large cells (pass the gate), a few NaN corners at the edges to
  // simulate reprojection holes. The sampler must ignore the NaN cells and
  // decide from the valid majority.
  auto coords = make_grid(11, 11, 100.0);
  const double nan_val = std::numeric_limits<double>::quiet_NaN();
  coords.set(0, 0, nan_val, nan_val);
  coords.set(10, 10, nan_val, nan_val);

  auto box = make_box(100.0, 100, 100);  // pixel = 1.0, cells = 10.0 → ratio 10

  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, box), 4);
}

BOOST_AUTO_TEST_CASE(all_samples_nan_returns_zero)
{
  Fmi::CoordinateMatrix coords(10, 10);  // default-constructed grid: all NaN
  const double nan_val = std::numeric_limits<double>::quiet_NaN();
  for (std::size_t j = 0; j < 10; ++j)
    for (std::size_t i = 0; i < 10; ++i)
      coords.set(i, j, nan_val, nan_val);

  auto box = make_box(100.0, 100, 100);

  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, box), 0);
}

BOOST_AUTO_TEST_CASE(zero_sized_pixel_viewport_returns_requested)
{
  auto coords = make_grid(11, 11, 100.0);
  Fmi::Box empty_box(0.0, 0.0, 100.0, 100.0);  // identity constructor: width=height=0

  // When we cannot determine the pixel size we cannot decide whether cells
  // are sub-pixel, so the gate gives the caller the benefit of the doubt and
  // returns the requested value unchanged. In the WMS render path a zero-
  // sized viewport never actually reaches contouring, so this branch exists
  // only as a defensive fall-back.
  BOOST_CHECK_EQUAL(effective_subdivide(4, 2.0, coords, empty_box), 4);
}
