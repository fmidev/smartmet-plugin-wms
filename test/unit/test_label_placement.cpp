// ======================================================================
// Unit tests for LabelPlacement geometry and algorithms.
//
// Compiled standalone against LabelPlacement.cpp with no SmartMet
// engine dependency.  Run with: make test
//
// Tests cover:
//   1. LabelBBox::overlaps — axis-aligned overlap detection
//   2. LabelBBox::overlap_area — overlap area computation
//   3. candidateBBoxes — Imhof position geometry
//   4. Fixed placement — label at requested position
//   5. Greedy placement — zero mutual overlap guarantee
//   6. Priority-greedy — highest-population labels placed first
//   7. Simulated annealing — energy does not increase overall
// ======================================================================

#define BOOST_TEST_MODULE LabelPlacementTest
#include <boost/test/unit_test.hpp>

#include "LabelPlacement.h"

#include <cmath>
#include <numeric>
#include <string>
#include <vector>

using namespace SmartMet::Plugin::Dali;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static LabelConfig makeConfig(PlacementAlgorithm algo,
                               int candidates = 8,
                               double offset = 5.0)
{
  LabelConfig cfg;
  cfg.algorithm = algo;
  cfg.candidates = candidates;
  cfg.offset = offset;
  cfg.font_family = "sans-serif";
  cfg.font_size = 11;
  cfg.font_weight = "normal";
  cfg.fill = "#333333";
  cfg.stroke = "white";
  cfg.stroke_width = 2.0;
  cfg.stroke_opacity = 0.75;
  cfg.max_labels = 500;
  return cfg;
}

static LabelCandidate makeCandidate(double ax, double ay,
                                    const std::string& name = "City",
                                    double pop = 10000,
                                    unsigned int w = 40,
                                    unsigned int h = 12)
{
  LabelCandidate c;
  c.anchor_x = ax;
  c.anchor_y = ay;
  c.text = name;
  c.population = pop;
  c.label_w = w;
  c.label_h = h;
  c.font_size = 11;
  c.font_weight = "normal";
  c.fill = "#333333";
  return c;
}

// Count placed labels in a result
static int countPlaced(const std::vector<PlacedLabel>& result)
{
  int n = 0;
  for (const auto& pl : result)
    if (pl.placed)
      n++;
  return n;
}

// True if any two placed labels overlap each other
static bool anyOverlap(const std::vector<PlacedLabel>& result)
{
  std::vector<LabelBBox> bboxes;
  for (const auto& pl : result)
    if (pl.placed)
      bboxes.push_back(pl.bbox);

  for (size_t i = 0; i < bboxes.size(); i++)
    for (size_t j = i + 1; j < bboxes.size(); j++)
      if (bboxes[i].overlaps(bboxes[j]))
        return true;
  return false;
}

// ======================================================================
// 1. LabelBBox::overlaps
// ======================================================================

BOOST_AUTO_TEST_SUITE(bbox_overlaps)

BOOST_AUTO_TEST_CASE(identical_boxes_overlap)
{
  LabelBBox a{0, 0, 10, 10};
  BOOST_CHECK(a.overlaps(a));
}

BOOST_AUTO_TEST_CASE(disjoint_right_no_overlap)
{
  LabelBBox a{0, 0, 10, 10};
  LabelBBox b{10, 0, 20, 10};  // touching edge, not overlapping
  BOOST_CHECK(!a.overlaps(b));
  BOOST_CHECK(!b.overlaps(a));
}

BOOST_AUTO_TEST_CASE(disjoint_below_no_overlap)
{
  LabelBBox a{0, 0, 10, 10};
  LabelBBox b{0, 10, 10, 20};
  BOOST_CHECK(!a.overlaps(b));
}

BOOST_AUTO_TEST_CASE(partial_overlap)
{
  LabelBBox a{0, 0, 10, 10};
  LabelBBox b{5, 5, 15, 15};
  BOOST_CHECK(a.overlaps(b));
  BOOST_CHECK(b.overlaps(a));
}

BOOST_AUTO_TEST_CASE(contained_box_overlaps)
{
  LabelBBox outer{0, 0, 20, 20};
  LabelBBox inner{5, 5, 15, 15};
  BOOST_CHECK(outer.overlaps(inner));
  BOOST_CHECK(inner.overlaps(outer));
}

BOOST_AUTO_TEST_CASE(far_apart_no_overlap)
{
  LabelBBox a{0, 0, 10, 10};
  LabelBBox b{100, 100, 110, 110};
  BOOST_CHECK(!a.overlaps(b));
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 2. LabelBBox::overlap_area
// ======================================================================

BOOST_AUTO_TEST_SUITE(bbox_overlap_area)

BOOST_AUTO_TEST_CASE(no_overlap_area_is_zero)
{
  LabelBBox a{0, 0, 10, 10};
  LabelBBox b{10, 0, 20, 10};
  BOOST_CHECK_SMALL(a.overlap_area(b), 1e-9);
}

BOOST_AUTO_TEST_CASE(identical_box_full_area)
{
  LabelBBox a{0, 0, 10, 10};
  BOOST_CHECK_CLOSE(a.overlap_area(a), 100.0, 1e-9);
}

BOOST_AUTO_TEST_CASE(partial_overlap_area)
{
  LabelBBox a{0, 0, 10, 10};
  LabelBBox b{5, 5, 15, 15};
  // Intersection is [5,10] x [5,10] = 5*5 = 25
  BOOST_CHECK_CLOSE(a.overlap_area(b), 25.0, 1e-9);
  BOOST_CHECK_CLOSE(b.overlap_area(a), 25.0, 1e-9);  // symmetric
}

BOOST_AUTO_TEST_CASE(contained_box_area_equals_inner)
{
  LabelBBox outer{0, 0, 20, 20};
  LabelBBox inner{5, 5, 15, 15};
  // Intersection is the inner box: 10*10 = 100
  BOOST_CHECK_CLOSE(outer.overlap_area(inner), 100.0, 1e-9);
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 3. candidateBBoxes — geometry invariants
// ======================================================================

BOOST_AUTO_TEST_SUITE(candidate_bboxes)

BOOST_AUTO_TEST_CASE(returns_requested_count_4)
{
  auto bboxes = candidateBBoxes(100, 100, 40, 12, 5, 4);
  BOOST_CHECK_EQUAL(bboxes.size(), 4u);
}

BOOST_AUTO_TEST_CASE(returns_requested_count_8)
{
  auto bboxes = candidateBBoxes(100, 100, 40, 12, 5, 8);
  BOOST_CHECK_EQUAL(bboxes.size(), 8u);
}

BOOST_AUTO_TEST_CASE(returns_16_for_large_k)
{
  auto bboxes = candidateBBoxes(100, 100, 40, 12, 5, 16);
  BOOST_CHECK_EQUAL(bboxes.size(), 16u);
  // Clamped to 16 even if caller asks for more
  auto bboxes32 = candidateBBoxes(100, 100, 40, 12, 5, 32);
  BOOST_CHECK_EQUAL(bboxes32.size(), 16u);
}

BOOST_AUTO_TEST_CASE(all_bboxes_have_correct_size)
{
  const unsigned int w = 40, h = 12;
  auto bboxes = candidateBBoxes(200, 200, w, h, 5, 8);
  for (const auto& b : bboxes)
  {
    BOOST_CHECK_CLOSE(b.x2 - b.x1, static_cast<double>(w), 1e-9);
    BOOST_CHECK_CLOSE(b.y2 - b.y1, static_cast<double>(h), 1e-9);
  }
}

BOOST_AUTO_TEST_CASE(ne_is_first_and_upper_right_of_anchor)
{
  // anchor at (100, 100), label 40x12, gap 5
  const double ax = 100, ay = 100;
  const unsigned int w = 40, h = 12;
  const double offset = 5;
  auto bboxes = candidateBBoxes(ax, ay, w, h, offset, 8);

  // NE candidate (index 0): x1 > ax (to the right)
  BOOST_CHECK_GT(bboxes[0].x1, ax);
  // NE candidate: y2 < ay (above the anchor in SVG coords)
  BOOST_CHECK_LT(bboxes[0].y2, ay);
}

BOOST_AUTO_TEST_CASE(se_is_second_and_lower_right)
{
  auto bboxes = candidateBBoxes(100, 100, 40, 12, 5, 8);
  // SE (index 1): x1 > ax, y1 > ay (right preferred over above wins tiebreak)
  BOOST_CHECK_GT(bboxes[1].x1, 100.0);
  BOOST_CHECK_GT(bboxes[1].y1, 100.0);
}

BOOST_AUTO_TEST_CASE(nw_is_third_and_upper_left)
{
  auto bboxes = candidateBBoxes(100, 100, 40, 12, 5, 8);
  // NW (index 2): x2 < ax, y2 < ay
  BOOST_CHECK_LT(bboxes[2].x2, 100.0);
  BOOST_CHECK_LT(bboxes[2].y2, 100.0);
}

BOOST_AUTO_TEST_CASE(all_bboxes_are_valid)
{
  auto bboxes = candidateBBoxes(300, 300, 50, 14, 6, 16);
  for (const auto& b : bboxes)
    BOOST_CHECK(b.valid());
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 4. Fixed placement
// ======================================================================

BOOST_AUTO_TEST_SUITE(fixed_placement)

BOOST_AUTO_TEST_CASE(single_label_placed)
{
  auto cfg = makeConfig(PlacementAlgorithm::Fixed);
  cfg.fixed_position = "ne";
  std::vector<LabelCandidate> cands = {makeCandidate(100, 100)};

  auto result = placeLabels(cfg, cands, 500, 500);
  BOOST_REQUIRE_EQUAL(result.size(), 1u);
  BOOST_CHECK(result[0].placed);
}

BOOST_AUTO_TEST_CASE(ne_label_is_upper_right_of_anchor)
{
  auto cfg = makeConfig(PlacementAlgorithm::Fixed);
  cfg.fixed_position = "ne";
  auto result = placeLabels(cfg, {makeCandidate(200, 200, "X", 1000, 40, 12)}, 500, 500);
  BOOST_CHECK(result[0].placed);
  BOOST_CHECK_GT(result[0].bbox.x1, 200.0);  // to the right
  BOOST_CHECK_LT(result[0].bbox.y2, 200.0);  // above
}

BOOST_AUTO_TEST_CASE(sw_label_is_lower_left_of_anchor)
{
  auto cfg = makeConfig(PlacementAlgorithm::Fixed);
  cfg.fixed_position = "sw";
  auto result = placeLabels(cfg, {makeCandidate(200, 200, "X", 1000, 40, 12)}, 500, 500);
  BOOST_CHECK(result[0].placed);
  BOOST_CHECK_LT(result[0].bbox.x2, 200.0);  // to the left
  BOOST_CHECK_GT(result[0].bbox.y1, 200.0);  // below
}

BOOST_AUTO_TEST_CASE(unknown_position_defaults_to_ne)
{
  auto cfg = makeConfig(PlacementAlgorithm::Fixed);
  cfg.fixed_position = "zzz";  // unknown
  auto cfg_ne = makeConfig(PlacementAlgorithm::Fixed);
  cfg_ne.fixed_position = "ne";

  auto r1 = placeLabels(cfg,    {makeCandidate(100, 100, "A", 1, 30, 10)}, 500, 500);
  auto r2 = placeLabels(cfg_ne, {makeCandidate(100, 100, "A", 1, 30, 10)}, 500, 500);
  BOOST_CHECK_EQUAL(r1[0].bbox.x1, r2[0].bbox.x1);
  BOOST_CHECK_EQUAL(r1[0].bbox.y1, r2[0].bbox.y1);
}

BOOST_AUTO_TEST_CASE(fixed_does_not_drop_labels_for_overlaps)
{
  // Two labels at the same anchor: fixed always places them, even if overlapping
  auto cfg = makeConfig(PlacementAlgorithm::Fixed);
  cfg.fixed_position = "ne";
  std::vector<LabelCandidate> cands = {
      makeCandidate(100, 100, "A", 1),
      makeCandidate(100, 100, "B", 2)};
  auto result = placeLabels(cfg, cands, 500, 500);
  BOOST_CHECK_EQUAL(countPlaced(result), 2);
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 5. Greedy placement — mutual non-overlap guarantee
// ======================================================================

BOOST_AUTO_TEST_SUITE(greedy_placement)

BOOST_AUTO_TEST_CASE(single_label_placed)
{
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  auto result = placeLabels(cfg, {makeCandidate(100, 100)}, 500, 500);
  BOOST_REQUIRE_EQUAL(result.size(), 1u);
  BOOST_CHECK(result[0].placed);
}

BOOST_AUTO_TEST_CASE(no_mutual_overlap_grid)
{
  // 5x5 grid of well-separated anchors: all should be placed without overlap
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  std::vector<LabelCandidate> cands;
  for (int row = 0; row < 5; row++)
    for (int col = 0; col < 5; col++)
      cands.push_back(makeCandidate(80.0 + col * 100, 80.0 + row * 120,
                                    "City", 5000, 40, 12));

  auto result = placeLabels(cfg, cands, 600, 700);
  BOOST_CHECK(!anyOverlap(result));
}

BOOST_AUTO_TEST_CASE(dense_cluster_drops_not_overlaps)
{
  // Many anchors at the same pixel position: most labels must be dropped,
  // but those that ARE placed must not overlap each other.
  auto cfg = makeConfig(PlacementAlgorithm::Greedy, 8, 3.0);
  std::vector<LabelCandidate> cands;
  for (int i = 0; i < 20; i++)
    cands.push_back(makeCandidate(200, 200, "C" + std::to_string(i), 1, 50, 14));

  auto result = placeLabels(cfg, cands, 500, 500);
  BOOST_CHECK(!anyOverlap(result));
  // At most a handful can be placed around one anchor
  BOOST_CHECK_LT(countPlaced(result), 20);
}

BOOST_AUTO_TEST_CASE(no_overlap_with_map_bounds)
{
  // Anchor near corner — all labels within map bounds
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  auto result = placeLabels(cfg, {makeCandidate(5, 5, "Corner", 1, 40, 12)}, 200, 200);
  for (const auto& pl : result)
    if (pl.placed)
    {
      BOOST_CHECK_GE(pl.bbox.x1, 0.0);
      BOOST_CHECK_GE(pl.bbox.y1, 0.0);
      BOOST_CHECK_LE(pl.bbox.x2, 200.0);
      BOOST_CHECK_LE(pl.bbox.y2, 200.0);
    }
}

BOOST_AUTO_TEST_CASE(no_label_placed_outside_map)
{
  auto cfg = makeConfig(PlacementAlgorithm::Greedy, 8, 100.0);
  // Very large offset and tiny map: no candidate can fit within bounds,
  // so the result must contain exactly zero placed labels.
  auto result = placeLabels(cfg, {makeCandidate(50, 50, "X", 1, 40, 12)}, 60, 60);
  BOOST_CHECK_EQUAL(countPlaced(result), 0);
}

BOOST_AUTO_TEST_CASE(max_labels_cap_respected)
{
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  cfg.max_labels = 3;
  std::vector<LabelCandidate> cands;
  for (int i = 0; i < 10; i++)
    cands.push_back(makeCandidate(100.0 + i * 80, 100, "L", 1, 30, 10));

  auto result = placeLabels(cfg, cands, 1000, 500);
  BOOST_CHECK_EQUAL(result.size(), 3u);
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 6. Priority-greedy — population ordering
// ======================================================================

BOOST_AUTO_TEST_SUITE(priority_greedy_placement)

BOOST_AUTO_TEST_CASE(no_mutual_overlap)
{
  auto cfg = makeConfig(PlacementAlgorithm::PriorityGreedy);
  std::vector<LabelCandidate> cands;
  for (int i = 0; i < 4; i++)
    cands.push_back(makeCandidate(80.0 + i * 120, 200, "C", 1000 * (i + 1), 40, 12));

  auto result = placeLabels(cfg, cands, 600, 400);
  BOOST_CHECK(!anyOverlap(result));
}

BOOST_AUTO_TEST_CASE(high_population_placed_over_low_in_conflict)
{
  // Two anchors at the same position: one has much larger population.
  // The high-population one must be placed; the low-population one is dropped.
  auto cfg = makeConfig(PlacementAlgorithm::PriorityGreedy, 8, 3.0);

  // 8 identical-position anchors with varying population.
  // After priority sort the highest-pop city grabs the best position.
  std::vector<LabelCandidate> cands;
  for (int i = 0; i < 8; i++)
    cands.push_back(makeCandidate(200, 200,
                                  "City" + std::to_string(i),
                                  static_cast<double>((8 - i) * 1000), // pop decreasing
                                  50, 14));

  // Reorder to put low-population first so that the geonames-order
  // greedy would place the wrong one.  Priority-greedy re-sorts, so
  // the highest-population (cands[0] after reversal) must be placed.
  std::reverse(cands.begin(), cands.end());  // now index 0 has lowest pop

  auto result = placeLabels(cfg, cands, 500, 500);

  // Find the highest-population placed label
  double max_placed_pop = 0;
  for (const auto& pl : result)
    if (pl.placed)
      max_placed_pop = std::max(max_placed_pop, pl.population);

  BOOST_CHECK_CLOSE(max_placed_pop, 8000.0, 1.0);
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 7. Simulated annealing
// ======================================================================

BOOST_AUTO_TEST_SUITE(simulated_annealing)

BOOST_AUTO_TEST_CASE(single_label_placed)
{
  auto cfg = makeConfig(PlacementAlgorithm::SimulatedAnnealing);
  auto result = placeLabels(cfg, {makeCandidate(100, 100)}, 500, 500);
  BOOST_REQUIRE_EQUAL(result.size(), 1u);
  BOOST_CHECK(result[0].placed);
}

BOOST_AUTO_TEST_CASE(result_size_matches_input)
{
  auto cfg = makeConfig(PlacementAlgorithm::SimulatedAnnealing);
  std::vector<LabelCandidate> cands;
  for (int i = 0; i < 10; i++)
    cands.push_back(makeCandidate(100.0 + i * 50, 200));
  auto result = placeLabels(cfg, cands, 700, 400);
  BOOST_CHECK_EQUAL(result.size(), cands.size());
}

BOOST_AUTO_TEST_CASE(placed_labels_are_within_map_bounds)
{
  auto cfg = makeConfig(PlacementAlgorithm::SimulatedAnnealing);
  std::vector<LabelCandidate> cands;
  for (int row = 0; row < 4; row++)
    for (int col = 0; col < 4; col++)
      cands.push_back(makeCandidate(60 + col * 80.0, 60 + row * 80.0,
                                    "L", 1000, 40, 12));
  const double W = 400, H = 400;
  auto result = placeLabels(cfg, cands, W, H);
  for (const auto& pl : result)
    if (pl.placed)
    {
      BOOST_CHECK_GE(pl.bbox.x1, 0.0);
      BOOST_CHECK_GE(pl.bbox.y1, 0.0);
      BOOST_CHECK_LE(pl.bbox.x2, W);
      BOOST_CHECK_LE(pl.bbox.y2, H);
    }
}

BOOST_AUTO_TEST_CASE(sa_reduces_total_overlap_vs_naive)
{
  // Compare SA against fixed-NE placement on a dense cluster.
  // SA should produce lower or equal total overlap energy.
  const double ax = 150, ay = 150;
  std::vector<LabelCandidate> cands;
  for (int i = 0; i < 6; i++)
    cands.push_back(makeCandidate(ax + i * 5.0, ay, "Lbl", 1000, 45, 13));

  auto cfg_sa = makeConfig(PlacementAlgorithm::SimulatedAnnealing);
  cfg_sa.sa_iterations = 5000;

  auto cfg_fixed = makeConfig(PlacementAlgorithm::Fixed);
  cfg_fixed.fixed_position = "ne";

  auto sa_result = placeLabels(cfg_sa, cands, 400, 400);
  auto fixed_result = placeLabels(cfg_fixed, cands, 400, 400);

  // Compute total pairwise overlap for SA
  double sa_overlap = 0;
  for (size_t i = 0; i < sa_result.size(); i++)
    for (size_t j = i + 1; j < sa_result.size(); j++)
      if (sa_result[i].placed && sa_result[j].placed)
        sa_overlap += sa_result[i].bbox.overlap_area(sa_result[j].bbox);

  // Compute total pairwise overlap for fixed-NE
  double fixed_overlap = 0;
  for (size_t i = 0; i < fixed_result.size(); i++)
    for (size_t j = i + 1; j < fixed_result.size(); j++)
      if (fixed_result[i].placed && fixed_result[j].placed)
        fixed_overlap += fixed_result[i].bbox.overlap_area(fixed_result[j].bbox);

  BOOST_CHECK_LE(sa_overlap, fixed_overlap + 1.0);  // SA ≤ fixed (with 1px tolerance)
}

BOOST_AUTO_TEST_CASE(deterministic_across_calls)
{
  // Identical inputs must produce bit-identical output (fixed RNG seed)
  auto cfg = makeConfig(PlacementAlgorithm::SimulatedAnnealing);
  std::vector<LabelCandidate> cands;
  for (int i = 0; i < 8; i++)
    cands.push_back(makeCandidate(50 + i * 30.0, 100, "X", 1000, 35, 11));

  auto r1 = placeLabels(cfg, cands, 400, 400);
  auto r2 = placeLabels(cfg, cands, 400, 400);

  BOOST_REQUIRE_EQUAL(r1.size(), r2.size());
  for (size_t i = 0; i < r1.size(); i++)
  {
    BOOST_CHECK_EQUAL(r1[i].placed, r2[i].placed);
    if (r1[i].placed)
    {
      BOOST_CHECK_EQUAL(r1[i].bbox.x1, r2[i].bbox.x1);
      BOOST_CHECK_EQUAL(r1[i].bbox.y1, r2[i].bbox.y1);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 8. LabelConfig style resolution
// ======================================================================

BOOST_AUTO_TEST_SUITE(label_config_style)

BOOST_AUTO_TEST_CASE(default_style_when_no_classes)
{
  LabelConfig cfg;
  cfg.font_size = 11;
  cfg.font_weight = "normal";
  cfg.fill = "#333";
  BOOST_CHECK_CLOSE(cfg.effective_font_size(50000), 11.0, 1e-9);
  BOOST_CHECK_EQUAL(cfg.effective_font_weight(50000), "normal");
  BOOST_CHECK_EQUAL(cfg.effective_fill(50000), "#333");
}

BOOST_AUTO_TEST_CASE(class_overrides_default)
{
  LabelConfig cfg;
  cfg.font_size = 11;
  cfg.font_weight = "normal";
  cfg.fill = "#333";

  LabelStyleClass big;
  big.lolimit = 100000;
  big.font_size = 14;
  big.font_weight = "bold";
  big.fill = "#000";
  cfg.classes.push_back(big);

  // Below threshold: defaults
  BOOST_CHECK_CLOSE(cfg.effective_font_size(50000), 11.0, 1e-9);
  BOOST_CHECK_EQUAL(cfg.effective_font_weight(50000), "normal");

  // Above threshold: class overrides
  BOOST_CHECK_CLOSE(cfg.effective_font_size(200000), 14.0, 1e-9);
  BOOST_CHECK_EQUAL(cfg.effective_font_weight(200000), "bold");
  BOOST_CHECK_EQUAL(cfg.effective_fill(200000), "#000");
}

BOOST_AUTO_TEST_CASE(first_matching_class_wins)
{
  LabelConfig cfg;
  cfg.font_size = 10;
  cfg.font_weight = "normal";
  cfg.fill = "#555";

  LabelStyleClass c1, c2;
  c1.lolimit = 50000;
  c1.font_size = 12;
  c2.lolimit = 10000;
  c2.font_size = 11;
  cfg.classes = {c1, c2};

  // population=80000 matches c1 first
  BOOST_CHECK_CLOSE(cfg.effective_font_size(80000), 12.0, 1e-9);
  // population=20000 falls through c1 (lolimit=50000 not met), matches c2
  BOOST_CHECK_CLOSE(cfg.effective_font_size(20000), 11.0, 1e-9);
}

BOOST_AUTO_TEST_SUITE_END()

// ======================================================================
// 9. Edge cases
// ======================================================================

BOOST_AUTO_TEST_SUITE(edge_cases)

BOOST_AUTO_TEST_CASE(empty_input_returns_empty)
{
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  auto result = placeLabels(cfg, {}, 500, 500);
  BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(none_algorithm_returns_empty)
{
  auto cfg = makeConfig(PlacementAlgorithm::None);
  auto result = placeLabels(cfg, {makeCandidate(100, 100)}, 500, 500);
  BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(zero_size_label_handled)
{
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  auto result = placeLabels(cfg, {makeCandidate(100, 100, "X", 1, 0, 0)}, 500, 500);
  BOOST_REQUIRE_EQUAL(result.size(), 1u);
  // Should be placed (zero-size labels always fit)
}

BOOST_AUTO_TEST_CASE(sa_empty_input_returns_empty)
{
  auto cfg = makeConfig(PlacementAlgorithm::SimulatedAnnealing);
  auto result = placeLabels(cfg, {}, 500, 500);
  BOOST_CHECK(result.empty());
}

// ----- Marker obstacle and bbox-padding regression tests -----------

BOOST_AUTO_TEST_CASE(symbol_extents_push_label_off_marker)
{
  // 16x16 marker, 5 px offset → label should be at least 5 px outside the
  // marker rectangle in every direction.
  auto bboxes = candidateBBoxes(100.0, 100.0, 30, 12, 5.0, 8, 8.0, 8.0);
  BOOST_REQUIRE_EQUAL(bboxes.size(), 8u);
  for (const auto& b : bboxes)
  {
    // Marker covers x in [92, 108] and y in [92, 108]
    const bool clear =
        b.x2 <= 92.0 || b.x1 >= 108.0 || b.y2 <= 92.0 || b.y1 >= 108.0;
    BOOST_CHECK_MESSAGE(clear, "label bbox not clear of marker rectangle");
  }
}

BOOST_AUTO_TEST_CASE(zero_symbol_extents_match_legacy_geometry)
{
  // No-symbol path must give the same bboxes as the previous behaviour
  // (offset / offset/2 spacing) so existing test outputs are stable.
  auto a = candidateBBoxes(100.0, 100.0, 30, 12, 5.0, 8);
  auto b = candidateBBoxes(100.0, 100.0, 30, 12, 5.0, 8, 0.0, 0.0);
  BOOST_REQUIRE_EQUAL(a.size(), b.size());
  for (size_t i = 0; i < a.size(); i++)
  {
    BOOST_CHECK_CLOSE(a[i].x1, b[i].x1, 1e-9);
    BOOST_CHECK_CLOSE(a[i].y1, b[i].y1, 1e-9);
    BOOST_CHECK_CLOSE(a[i].x2, b[i].x2, 1e-9);
    BOOST_CHECK_CLOSE(a[i].y2, b[i].y2, 1e-9);
  }
}

BOOST_AUTO_TEST_CASE(greedy_avoids_other_marker_obstacle)
{
  // Two labels placed close enough that the second label's chosen
  // position would land on the first label's marker without the obstacle
  // term in greedyCore.  Radial placement may put a label inside its
  // *own* marker's bbox corner (between the inscribed circle and the
  // square corner) — that is allowed and explicitly skipped in greedy —
  // but it must never land inside *another* candidate's obstacle.
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  cfg.symbol_width = 30;
  cfg.symbol_height = 30;

  auto a = makeCandidate(100, 100, "A", 1000, 20, 10);
  a.obstacle = {85.0, 85.0, 115.0, 115.0};
  auto b = makeCandidate(150, 100, "B", 1000, 20, 10);
  b.obstacle = {135.0, 85.0, 165.0, 115.0};

  auto result = placeLabels(cfg, {a, b}, 500, 500);
  BOOST_REQUIRE_EQUAL(result.size(), 2u);
  if (result[0].placed)
    BOOST_CHECK(!result[0].bbox.overlaps(b.obstacle));
  if (result[1].placed)
    BOOST_CHECK(!result[1].bbox.overlaps(a.obstacle));
}

BOOST_AUTO_TEST_CASE(inflated_bbox_helper_extends_each_edge)
{
  LabelBBox a{10.0, 20.0, 30.0, 40.0};
  auto b = a.inflated(3.0);
  BOOST_CHECK_CLOSE(b.x1, 7.0, 1e-9);
  BOOST_CHECK_CLOSE(b.y1, 17.0, 1e-9);
  BOOST_CHECK_CLOSE(b.x2, 33.0, 1e-9);
  BOOST_CHECK_CLOSE(b.y2, 43.0, 1e-9);
}

BOOST_AUTO_TEST_CASE(bbox_padding_changes_overlap_decision)
{
  // Two labels separated by ~5 px between their NE bboxes.
  // With padding > 2.5 the inflated bboxes touch — second label must
  // pick a different position than it does without padding.
  auto cfg_no_pad = makeConfig(PlacementAlgorithm::Greedy);
  cfg_no_pad.bbox_padding = 0.0;
  auto cfg_pad = cfg_no_pad;
  cfg_pad.bbox_padding = 6.0;

  // a NE = (105, 88, 125, 100), b NE = (135, 88, 155, 100); 10 px gap
  auto a = makeCandidate(100, 100, "A", 1000, 20, 10);
  auto b = makeCandidate(130, 100, "B", 1000, 20, 10);

  auto r0 = placeLabels(cfg_no_pad, {a, b}, 500, 500);
  auto r1 = placeLabels(cfg_pad, {a, b}, 500, 500);
  BOOST_REQUIRE_EQUAL(r0.size(), 2u);
  BOOST_REQUIRE_EQUAL(r1.size(), 2u);

  // Without padding both fit in NE.  With padding the second label
  // must use a different bbox or be dropped.
  BOOST_REQUIRE(r0[1].placed);
  if (r1[1].placed)
  {
    const bool changed =
        r0[1].bbox.x1 != r1[1].bbox.x1 || r0[1].bbox.y1 != r1[1].bbox.y1;
    BOOST_CHECK(changed);
  }
}

BOOST_AUTO_TEST_CASE(priority_greedy_returns_input_order)
{
  // priority-greedy sorts internally by population, but the result
  // must come back in input order so callers can correlate result[i]
  // with input[i] (e.g. to suppress the marker for a dropped label).
  auto cfg = makeConfig(PlacementAlgorithm::PriorityGreedy);

  // Input order intentionally not sorted by population
  std::vector<LabelCandidate> cands = {
      makeCandidate(100, 100, "Small",  100, 30, 10),
      makeCandidate(200, 100, "Big",  500000, 30, 10),
      makeCandidate(300, 100, "Mid",   10000, 30, 10),
  };

  auto result = placeLabels(cfg, cands, 500, 500);
  BOOST_REQUIRE_EQUAL(result.size(), 3u);
  BOOST_CHECK_EQUAL(result[0].text, "Small");
  BOOST_CHECK_EQUAL(result[1].text, "Big");
  BOOST_CHECK_EQUAL(result[2].text, "Mid");
}

BOOST_AUTO_TEST_CASE(bbox_padding_does_not_clip_against_map_bounds)
{
  // A label whose natural bbox sits exactly at the right map edge must
  // remain placeable when bbox_padding > 0 — padding is for inter-label
  // collision only, never a hard map-bounds constraint.
  auto cfg = makeConfig(PlacementAlgorithm::Greedy);
  cfg.bbox_padding = 5.0;

  // Anchor at x=470 with a 20-wide label: NE bbox extends to ~495.
  auto c = makeCandidate(470, 100, "Edge", 1000, 20, 10);
  auto result = placeLabels(cfg, {c}, 500, 500);
  BOOST_REQUIRE_EQUAL(result.size(), 1u);
  BOOST_CHECK(result[0].placed);
}

BOOST_AUTO_TEST_SUITE_END()
