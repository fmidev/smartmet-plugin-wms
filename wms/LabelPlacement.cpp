// ======================================================================

#include "LabelPlacement.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <random>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// ======================================================================
// LabelBBox
// ======================================================================

bool LabelBBox::overlaps(const LabelBBox& o) const noexcept
{
  return x1 < o.x2 && x2 > o.x1 && y1 < o.y2 && y2 > o.y1;
}

double LabelBBox::overlap_area(const LabelBBox& o) const noexcept
{
  const double dx = std::min(x2, o.x2) - std::max(x1, o.x1);
  const double dy = std::min(y2, o.y2) - std::max(y1, o.y1);
  if (dx <= 0.0 || dy <= 0.0)
    return 0.0;
  return dx * dy;
}

// ======================================================================
// LabelStyleClass
// ======================================================================

bool LabelStyleClass::matches(double population) const noexcept
{
  if (lolimit && population < *lolimit)
    return false;
  if (hilimit && population >= *hilimit)
    return false;
  return true;
}

// ======================================================================
// LabelConfig style resolution
// ======================================================================

double LabelConfig::effective_font_size(double population) const noexcept
{
  for (const auto& cls : classes)
    if (cls.matches(population) && cls.font_size > 0)
      return cls.font_size;
  return font_size;
}

std::string LabelConfig::effective_font_weight(double population) const
{
  for (const auto& cls : classes)
    if (cls.matches(population) && !cls.font_weight.empty())
      return cls.font_weight;
  return font_weight;
}

std::string LabelConfig::effective_fill(double population) const
{
  for (const auto& cls : classes)
    if (cls.matches(population) && !cls.fill.empty())
      return cls.fill;
  return fill;
}

// ======================================================================
// Candidate bounding-box geometry
// ======================================================================

// Eight standard positions in Imhof (1975) preference order.
// Indices 0-7 are used for num_candidates <= 8; all 16 for larger values.
// The coordinate system has y increasing downward (SVG/pixel space).
//
// Legend: anchor (ax, ay) is the symbol centre; label bbox top-left is
// computed from (ax, ay), label size (w, h), and gap (offset).
//
// Preference order (best = 0):
//   0 NE  upper-right  — Imhof's strongly preferred position
//   1 NW  upper-left
//   2 SE  lower-right
//   3 SW  lower-left
//   4 E   right-middle
//   5 W   left-middle
//   6 N   top-centre   — partially obscures symbol, avoid if possible
//   7 S   bottom-centre — worst: below symbol
//
// The 16-candidate set inserts intermediate positions between the 8 above.

std::vector<LabelBBox> candidateBBoxes(double ax,
                                       double ay,
                                       unsigned int w,
                                       unsigned int h,
                                       double offset,
                                       int num_candidates)
{
  const auto dw = static_cast<double>(w);
  const auto dh = static_cast<double>(h);
  const double half_w = dw / 2.0;
  const double half_h = dh / 2.0;
  const double gap = offset;
  const double half_gap = offset / 2.0;

  // 8-position array in Imhof preference order.
  // Each entry: top-left (x1, y1) of the label bounding box.
  const std::array<std::pair<double, double>, 8> pos8{{
      {ax + gap,          ay - dh - half_gap},  // 0: NE
      {ax - dw - gap,     ay - dh - half_gap},  // 1: NW
      {ax + gap,          ay + half_gap},         // 2: SE
      {ax - dw - gap,     ay + half_gap},         // 3: SW
      {ax + gap,          ay - half_h},           // 4: E
      {ax - dw - gap,     ay - half_h},           // 5: W
      {ax - half_w,       ay - dh - gap},         // 6: N
      {ax - half_w,       ay + gap},               // 7: S
  }};

  // 16-position array: interleave 8 intermediate positions between
  // each pair of adjacent cardinal/ordinal positions.
  const std::array<std::pair<double, double>, 16> pos16{{
      {ax + gap,          ay - dh - half_gap},   // 0:  NE
      {ax - dw - gap,     ay - dh - half_gap},   // 1:  NW
      {ax + gap,          ay + half_gap},          // 2:  SE
      {ax - dw - gap,     ay + half_gap},          // 3:  SW
      {ax + gap,          ay - dh * 0.75},         // 4:  ENE
      {ax - dw - gap,     ay - dh * 0.75},         // 5:  WNW
      {ax + gap,          ay - dh * 0.25},         // 6:  ESE
      {ax - dw - gap,     ay - dh * 0.25},         // 7:  WSW
      {ax + gap,          ay - half_h},            // 8:  E
      {ax - dw - gap,     ay - half_h},            // 9:  W
      {ax - half_w,       ay - dh - gap},          // 10: N
      {ax - half_w,       ay + gap},                // 11: S
      {ax - dw * 0.25,    ay - dh - gap},          // 12: NNE
      {ax - dw * 0.75,    ay - dh - gap},          // 13: NNW
      {ax - dw * 0.25,    ay + gap},                // 14: SSE
      {ax - dw * 0.75,    ay + gap},                // 15: SSW
  }};

  // Clamp to supported counts: 4, 8, or 16
  int n = num_candidates;
  if (n <= 4)
    n = 4;
  else if (n <= 8)
    n = 8;
  else
    n = 16;

  std::vector<LabelBBox> result;
  result.reserve(static_cast<size_t>(n));

  if (n <= 8)
  {
    for (int i = 0; i < n; i++)
    {
      const auto& p = pos8[static_cast<size_t>(i)];
      result.push_back({p.first, p.second, p.first + dw, p.second + dh});
    }
  }
  else
  {
    for (int i = 0; i < n; i++)
    {
      const auto& p = pos16[static_cast<size_t>(i)];
      result.push_back({p.first, p.second, p.first + dw, p.second + dh});
    }
  }

  return result;
}

// ======================================================================
// Algorithm implementations
// ======================================================================

namespace
{

// ------------------------------------------------------------------
// Map bounds check
// ------------------------------------------------------------------
bool withinBounds(const LabelBBox& b, double map_w, double map_h)
{
  return b.x1 >= 0.0 && b.y1 >= 0.0 && b.x2 <= map_w && b.y2 <= map_h;
}

// ------------------------------------------------------------------
// Convert fixed_position string to candidate index (Imhof order)
// ------------------------------------------------------------------
int fixedPositionIndex(const std::string& pos)
{
  if (pos == "ne" || pos == "NE") return 0;
  if (pos == "nw" || pos == "NW") return 1;
  if (pos == "se" || pos == "SE") return 2;
  if (pos == "sw" || pos == "SW") return 3;
  if (pos == "e"  || pos == "E")  return 4;
  if (pos == "w"  || pos == "W")  return 5;
  if (pos == "n"  || pos == "N")  return 6;
  if (pos == "s"  || pos == "S")  return 7;
  return 0;  // unknown → NE
}

// ------------------------------------------------------------------
// Build a PlacedLabel from a LabelCandidate + chosen bbox
// ------------------------------------------------------------------
PlacedLabel makePlaced(const LabelCandidate& cand, const LabelBBox& bbox)
{
  PlacedLabel pl;
  pl.bbox = bbox;
  pl.text = cand.text;
  pl.population = cand.population;
  pl.font_size = cand.font_size;
  pl.font_weight = cand.font_weight;
  pl.fill = cand.fill;
  pl.placed = true;
  return pl;
}

PlacedLabel makeDropped(const LabelCandidate& cand)
{
  PlacedLabel pl;
  pl.text = cand.text;
  pl.population = cand.population;
  pl.font_size = cand.font_size;
  pl.font_weight = cand.font_weight;
  pl.fill = cand.fill;
  pl.placed = false;
  return pl;
}

// ------------------------------------------------------------------
// FIXED placement — one predetermined position, no conflict detection
// ------------------------------------------------------------------
std::vector<PlacedLabel> fixedPlacement(const LabelConfig& config,
                                        const std::vector<LabelCandidate>& candidates,
                                        double /*map_w*/, double /*map_h*/)
{
  const int pos_idx = fixedPositionIndex(config.fixed_position);

  std::vector<PlacedLabel> result;
  result.reserve(candidates.size());

  for (const auto& cand : candidates)
  {
    auto bboxes = candidateBBoxes(cand.anchor_x, cand.anchor_y,
                                  cand.label_w, cand.label_h,
                                  config.offset, 8);
    const int idx = std::min(pos_idx, static_cast<int>(bboxes.size()) - 1);
    result.push_back(makePlaced(cand, bboxes[static_cast<size_t>(idx)]));
  }
  return result;
}

// ------------------------------------------------------------------
// GREEDY core — tries candidate positions in Imhof preference order
// and takes the first one that does not overlap any already-placed
// label and is within map bounds.  Input order defines priority.
// ------------------------------------------------------------------
std::vector<PlacedLabel> greedyCore(const LabelConfig& config,
                                    const std::vector<LabelCandidate>& candidates,
                                    double map_w, double map_h)
{
  std::vector<PlacedLabel> result;
  result.reserve(candidates.size());

  // Flat list of placed bounding boxes for O(n·k·m) overlap check.
  // For typical weather-map label counts (<200) this is fast enough.
  std::vector<LabelBBox> placed_bboxes;
  placed_bboxes.reserve(candidates.size());

  for (const auto& cand : candidates)
  {
    auto bboxes = candidateBBoxes(cand.anchor_x, cand.anchor_y,
                                  cand.label_w, cand.label_h,
                                  config.offset, config.candidates);
    bool placed = false;
    for (const auto& bbox : bboxes)
    {
      if (!withinBounds(bbox, map_w, map_h))
        continue;

      bool overlap = false;
      for (const auto& pb : placed_bboxes)
      {
        if (bbox.overlaps(pb))
        {
          overlap = true;
          break;
        }
      }
      if (!overlap)
      {
        result.push_back(makePlaced(cand, bbox));
        placed_bboxes.push_back(bbox);
        placed = true;
        break;
      }
    }
    if (!placed)
      result.push_back(makeDropped(cand));
  }
  return result;
}

// ------------------------------------------------------------------
// GREEDY — uses geonames sort order directly (higher priority first)
// ------------------------------------------------------------------
std::vector<PlacedLabel> greedyPlacement(const LabelConfig& config,
                                         const std::vector<LabelCandidate>& candidates,
                                         double map_w, double map_h)
{
  return greedyCore(config, candidates, map_w, map_h);
}

// ------------------------------------------------------------------
// PRIORITY-GREEDY — re-sorts by population descending, then greedy
// ------------------------------------------------------------------
std::vector<PlacedLabel> priorityGreedyPlacement(const LabelConfig& config,
                                                  std::vector<LabelCandidate> candidates,
                                                  double map_w, double map_h)
{
  // stable_sort preserves geonames order for equal populations
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const LabelCandidate& a, const LabelCandidate& b) {
                     return a.population > b.population;
                   });
  return greedyCore(config, candidates, map_w, map_h);
}

// ------------------------------------------------------------------
// SIMULATED ANNEALING
// Based on: Christensen, Marks & Shieber (1995),
//           ACM Transactions on Graphics 14(3), 203-232.
//
// Energy function:
//   E = overlap_weight * sum_{i<j} overlap_area(label_i, label_j)
//     + position_weight * sum_i candidate_index_i
//
// We use delta-energy updates: each step only recomputes the rows
// for the one label being moved → O(n) per step instead of O(n^2).
// Total: O(iterations * n) = O(2000 * 200) = 400 000 ops for typical maps.
//
// Seed is fixed (42) so that the result is reproducible and the layer
// cache (hash_value) is consistent across identical requests.
// ------------------------------------------------------------------
std::vector<PlacedLabel> simulatedAnnealingPlacement(const LabelConfig& config,
                                                      const std::vector<LabelCandidate>& candidates,
                                                      double map_w, double map_h)
{
  const int n = static_cast<int>(candidates.size());
  if (n == 0)
    return {};

  // Pre-compute all candidate bounding boxes for every label
  std::vector<std::vector<LabelBBox>> all_bboxes(static_cast<size_t>(n));
  for (int i = 0; i < n; i++)
  {
    all_bboxes[static_cast<size_t>(i)] =
        candidateBBoxes(candidates[static_cast<size_t>(i)].anchor_x,
                        candidates[static_cast<size_t>(i)].anchor_y,
                        candidates[static_cast<size_t>(i)].label_w,
                        candidates[static_cast<size_t>(i)].label_h,
                        config.offset,
                        config.candidates);
  }

  // assignment[i]: index into all_bboxes[i], or -1 if dropped
  std::vector<int> assignment(static_cast<size_t>(n), -1);

  // Initialise: each label at its first within-bounds candidate
  for (int i = 0; i < n; i++)
  {
    for (int k = 0; k < static_cast<int>(all_bboxes[static_cast<size_t>(i)].size()); k++)
    {
      if (withinBounds(all_bboxes[static_cast<size_t>(i)][static_cast<size_t>(k)], map_w, map_h))
      {
        assignment[static_cast<size_t>(i)] = k;
        break;
      }
    }
  }

  // Delta-energy: contribution of label i at candidate position k with
  // respect to all other currently assigned labels (excludes self)
  auto overlapContrib = [&](int i, int k) -> double {
    if (k < 0)
      return 0.0;
    const auto& bi = all_bboxes[static_cast<size_t>(i)][static_cast<size_t>(k)];
    double e = 0.0;
    for (int j = 0; j < n; j++)
    {
      if (j == i)
        continue;
      const int kj = assignment[static_cast<size_t>(j)];
      if (kj < 0)
        continue;
      e += bi.overlap_area(all_bboxes[static_cast<size_t>(j)][static_cast<size_t>(kj)]);
    }
    return e;
  };

  // Position penalty: dropped label costs (num_candidates) to discourage dropping
  const int num_cands = static_cast<int>(
      all_bboxes.empty() ? 0 : all_bboxes[0].size());

  auto posPenalty = [&](int k) -> double {
    return (k < 0) ? static_cast<double>(num_cands)
                   : static_cast<double>(k);
  };

  // Fixed seed for reproducibility: identical map requests must produce
  // identical output so that the layer cache (hash_value) works correctly.
  // NOLINTNEXTLINE(cert-msc51-cpp)
  std::mt19937 rng(42U);
  std::uniform_int_distribution<int> label_dist(0, n - 1);
  std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

  double T = config.sa_initial_temp;
  const double cool = config.sa_cooling_rate;
  const double ow = config.sa_overlap_weight;
  const double pw = config.sa_position_weight;

  for (int iter = 0; iter < config.sa_iterations; iter++)
  {
    const int i = label_dist(rng);
    const auto& bboxes_i = all_bboxes[static_cast<size_t>(i)];
    if (bboxes_i.empty())
      continue;

    // Draw a new candidate: 0 .. size-1 are positions, size = drop label
    std::uniform_int_distribution<int> cand_dist(0, static_cast<int>(bboxes_i.size()));
    const int drawn = cand_dist(rng);
    const bool drawn_is_drop = (drawn == static_cast<int>(bboxes_i.size()));
    const bool drawn_out_of_bounds =
        !drawn_is_drop && !withinBounds(bboxes_i[static_cast<size_t>(drawn)], map_w, map_h);
    const int new_k = (drawn_is_drop || drawn_out_of_bounds) ? -1 : drawn;

    const int old_k = assignment[static_cast<size_t>(i)];
    if (new_k == old_k)
      continue;

    const double dE =
        ow * (overlapContrib(i, new_k) - overlapContrib(i, old_k)) +
        pw * (posPenalty(new_k) - posPenalty(old_k));

    if (dE < 0.0 || prob_dist(rng) < std::exp(-dE / std::max(T, 1e-10)))
      assignment[static_cast<size_t>(i)] = new_k;

    T *= cool;
  }

  // Build result vector
  std::vector<PlacedLabel> result;
  result.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; i++)
  {
    const int k = assignment[static_cast<size_t>(i)];
    if (k >= 0)
      result.push_back(
          makePlaced(candidates[static_cast<size_t>(i)],
                     all_bboxes[static_cast<size_t>(i)][static_cast<size_t>(k)]));
    else
      result.push_back(makeDropped(candidates[static_cast<size_t>(i)]));
  }
  return result;
}

}  // anonymous namespace

// ======================================================================
// Public dispatch function
// ======================================================================

std::vector<PlacedLabel> placeLabels(const LabelConfig& config,
                                     std::vector<LabelCandidate> candidates,
                                     double map_width,
                                     double map_height)
{
  // Apply max_labels cap; geonames sort already puts highest-priority first
  if (static_cast<int>(candidates.size()) > config.max_labels)
    candidates.resize(static_cast<size_t>(config.max_labels));

  switch (config.algorithm)
  {
    case PlacementAlgorithm::None:
      return {};
    case PlacementAlgorithm::Fixed:
      return fixedPlacement(config, candidates, map_width, map_height);
    case PlacementAlgorithm::Greedy:
      return greedyPlacement(config, candidates, map_width, map_height);
    case PlacementAlgorithm::PriorityGreedy:
      return priorityGreedyPlacement(config, std::move(candidates), map_width, map_height);
    case PlacementAlgorithm::SimulatedAnnealing:
      return simulatedAnnealingPlacement(config, candidates, map_width, map_height);
  }
  return {};  // unreachable
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
