// ======================================================================

#include "LabelPlacement.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// ======================================================================
// Position rules — direction unit vector and label alignment per
// candidate position.  Shared between candidateBBoxes (which uses them
// to position the label rectangle) and the free-space biasing code in
// the placement algorithms (which uses just the directions).
// ======================================================================

namespace
{

struct PositionRule { double dx, dy, afx, afy; };

constexpr double S2 = 0.7071067811865476;    // sin/cos 45°
constexpr double S225 = 0.3826834323650898;  // sin 22.5°
constexpr double S675 = 0.9238795325112867;  // sin 67.5° = cos 22.5°

// 8-position priority order (right > above): NE, SE, NW, SW, E, W, N, S
constexpr std::array<PositionRule, 8> RULES_8{{
    { S2, -S2, 0.0, 1.0},  // 0 NE: bottom-left of label at directional anchor
    { S2,  S2, 0.0, 0.0},  // 1 SE: top-left
    {-S2, -S2, 1.0, 1.0},  // 2 NW: bottom-right
    {-S2,  S2, 1.0, 0.0},  // 3 SW: top-right
    { 1.0, 0.0, 0.0, 0.5}, // 4 E:  left-middle
    {-1.0, 0.0, 1.0, 0.5}, // 5 W:  right-middle
    { 0.0,-1.0, 0.5, 1.0}, // 6 N:  bottom-middle
    { 0.0, 1.0, 0.5, 0.0}, // 7 S:  top-middle
}};

// 16-position: same priority order plus 8 finer-resolution intermediates.
// Each intermediate snaps its alignment fraction to the nearest 8-pos rule
// so the closest point of the label is still ~ at the directional anchor.
constexpr std::array<PositionRule, 16> RULES_16{{
    { S2,   -S2,   0.0, 1.0},  // 0  NE
    { S2,    S2,   0.0, 0.0},  // 1  SE
    {-S2,   -S2,   1.0, 1.0},  // 2  NW
    {-S2,    S2,   1.0, 0.0},  // 3  SW
    { S675, -S225, 0.0, 1.0},  // 4  ENE (snap to NE alignment)
    { S675,  S225, 0.0, 0.0},  // 5  ESE (snap to SE alignment)
    {-S675, -S225, 1.0, 1.0},  // 6  WNW (snap to NW alignment)
    {-S675,  S225, 1.0, 0.0},  // 7  WSW (snap to SW alignment)
    { 1.0,   0.0,  0.0, 0.5},  // 8  E
    {-1.0,   0.0,  1.0, 0.5},  // 9  W
    { S225, -S675, 0.5, 1.0},  // 10 NNE (snap to N alignment)
    { S225,  S675, 0.5, 0.0},  // 11 SSE (snap to S alignment)
    {-S225, -S675, 0.5, 1.0},  // 12 NNW (snap to N alignment)
    {-S225,  S675, 0.5, 0.0},  // 13 SSW (snap to S alignment)
    { 0.0,  -1.0,  0.5, 1.0},  // 14 N
    { 0.0,   1.0,  0.5, 0.0},  // 15 S
}};

inline int clampNumCandidates(int n)
{
  if (n <= 4) return 4;
  if (n <= 8) return 8;
  return 16;
}

inline const PositionRule& positionRule(int n_candidates, int idx)
{
  return (n_candidates <= 8) ? RULES_8[static_cast<size_t>(idx)]
                              : RULES_16[static_cast<size_t>(idx)];
}

}  // anonymous namespace

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

double LabelConfig::effective_symbol_width(double population) const noexcept
{
  for (const auto& cls : classes)
    if (cls.matches(population) && cls.symbol_width > 0)
      return cls.symbol_width;
  return symbol_width;
}

double LabelConfig::effective_symbol_height(double population) const noexcept
{
  for (const auto& cls : classes)
    if (cls.matches(population) && cls.symbol_height > 0)
      return cls.symbol_height;
  return symbol_height;
}

// ======================================================================
// Candidate bounding-box geometry
// ======================================================================

// Per-candidate free-direction calculation is now done inline in
// greedyCore and simulatedAnnealingPlacement so that already-placed
// labels can contribute to the "occupied" footprint, not just markers.

namespace
{

// Sort position indices [0..n) by score ascending so the lowest-cost
// position comes first.  Score = position_index − weight * alignment
// where alignment = position_direction · free_direction (in [-1, 1]).
std::vector<int> orderedPositionIndices(int n_candidates,
                                        std::pair<double, double> free_dir,
                                        double free_space_weight)
{
  std::vector<int> order(static_cast<size_t>(n_candidates));
  std::iota(order.begin(), order.end(), 0);
  if (free_space_weight <= 0.0 ||
      (free_dir.first == 0.0 && free_dir.second == 0.0))
    return order;  // plain priority order

  std::vector<double> score(static_cast<size_t>(n_candidates));
  for (int i = 0; i < n_candidates; i++)
  {
    const auto& r = positionRule(n_candidates, i);
    const double alignment = r.dx * free_dir.first + r.dy * free_dir.second;
    score[static_cast<size_t>(i)] =
        static_cast<double>(i) - free_space_weight * alignment;
  }
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) {
                     return score[static_cast<size_t>(a)] <
                            score[static_cast<size_t>(b)];
                   });
  return order;
}

}  // anonymous namespace

// Eight standard positions in preference order.
// Indices 0-7 are used for num_candidates <= 8; all 16 for larger values.
// The coordinate system has y increasing downward (SVG/pixel space).
//
// Legend: anchor (ax, ay) is the symbol centre; label bbox top-left is
// computed from (ax, ay), label size (w, h), and gap (offset).
//
// Preference rules (applied in order):
//   1) east of the anchor preferred over west
//   2) above the anchor preferred over below
// This gives the corner ranking NE > SE > NW > SW (cf. Imhof 1975, who
// preferred above-over-right; we follow the convention more common in
// modern dynamic visualisation, e.g. arxiv:1209.5765).  Cardinals come
// after corners; vertical cardinals come last because they sit directly
// over the symbol centre line.
//
// Preference order (best = 0):
//   0 NE  upper-right
//   1 SE  lower-right
//   2 NW  upper-left
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
                                       int num_candidates,
                                       double symbol_half_w,
                                       double symbol_half_h)
{
  const auto dw = static_cast<double>(w);
  const auto dh = static_cast<double>(h);

  // Radial placement: each label is positioned so its closest point
  // (corner / edge midpoint, depending on direction) lies at distance
  // `offset` from the marker's rectangle boundary along the position's
  // direction vector.
  //
  // Treating the marker as an axis-aligned rectangle gives the same gap
  // from any side or corner — exactly `offset` for square markers, and
  // a small *additional* corner-bias for circles (the inscribed circle
  // tucks in at the corners, so visually corner labels look slightly
  // further than cardinal labels by a factor of (√2 − 1) × radius).
  //
  // For each direction (dx, dy), the ray-to-rectangle distance is
  //
  //   t = min(sym_half_w / |dx|, sym_half_h / |dy|)
  //
  // (∞ when the corresponding marker extent or direction component is
  // zero).  D = t + offset.  For symbol_half_* = 0 this collapses to a
  // fixed-distance-from-anchor placement with D = offset.
  auto rectRayDist = [&](double dx, double dy) -> double {
    const double abs_dx = std::abs(dx);
    const double abs_dy = std::abs(dy);
    const double inf = std::numeric_limits<double>::infinity();
    const double t_x = (abs_dx > 1e-12) ? symbol_half_w / abs_dx : inf;
    const double t_y = (abs_dy > 1e-12) ? symbol_half_h / abs_dy : inf;
    return std::min(t_x, t_y);
  };

  // Direction unit vectors and label alignment fractions live in
  // RULES_8 / RULES_16 at file scope.  See the comment block at the
  // top of this file for the meaning of (dx, dy, afx, afy).
  const int n = clampNumCandidates(num_candidates);

  std::vector<LabelBBox> result;
  result.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; i++)
  {
    const auto& r = positionRule(n, i);
    const double D = rectRayDist(r.dx, r.dy) + offset;
    const double apx = ax + D * r.dx;
    const double apy = ay + D * r.dy;
    const double x1 = apx - dw * r.afx;
    const double y1 = apy - dh * r.afy;
    result.push_back({x1, y1, x1 + dw, y1 + dh});
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
  if (pos == "se" || pos == "SE") return 1;
  if (pos == "nw" || pos == "NW") return 2;
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
  pl.y_bearing = cand.y_bearing;
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
  pl.y_bearing = cand.y_bearing;
  pl.placed = false;
  return pl;
}

// ------------------------------------------------------------------
// FIXED placement — one predetermined position, no conflict detection
// ------------------------------------------------------------------
// Derive symbol half-extents from a candidate's obstacle bbox.  The
// caller (LocationLayer) builds the obstacle from the per-population
// effective symbol size plus symbol_margin, so this is the single source
// of truth for the marker rectangle that the label must clear.
std::pair<double, double> candidateSymbolHalf(const LabelCandidate& c)
{
  if (!c.obstacle.valid())
    return {0.0, 0.0};
  return {(c.obstacle.x2 - c.obstacle.x1) / 2.0,
          (c.obstacle.y2 - c.obstacle.y1) / 2.0};
}

std::vector<PlacedLabel> fixedPlacement(const LabelConfig& config,
                                        const std::vector<LabelCandidate>& candidates,
                                        double /*map_w*/, double /*map_h*/)
{
  const int pos_idx = fixedPositionIndex(config.fixed_position);
  const double pad = config.bbox_padding;
  const size_t n = candidates.size();

  // Pre-compute the chosen-position bbox for every candidate, so we can
  // do bidirectional conflict checks below.
  std::vector<LabelBBox> chosen(n);
  for (size_t i = 0; i < n; i++)
  {
    const auto& cand = candidates[i];
    const auto [sym_half_w, sym_half_h] = candidateSymbolHalf(cand);
    auto bboxes = candidateBBoxes(cand.anchor_x, cand.anchor_y,
                                  cand.label_w, cand.label_h,
                                  config.offset, 8,
                                  sym_half_w, sym_half_h);
    const int idx = std::min(pos_idx, static_cast<int>(bboxes.size()) - 1);
    chosen[i] = bboxes[static_cast<size_t>(idx)];
  }

  // Process in input order (geonames priority — highest first) and
  // accept a candidate only if it is compatible with everything already
  // in the kept set.  "Fixed" still performs no label-vs-label conflict
  // detection — labels may overlap each other — but we won't allow a
  // label to cover, or be covered by, a higher-priority kept marker.
  // This is a directed-priority heuristic for the underlying conflict
  // graph; an exact maximum-independent-set solution would be NP-hard.
  std::vector<bool> kept(n, false);

  for (size_t i = 0; i < n; i++)
  {
    const auto& bi = chosen[i];
    const LabelBBox test_i = (pad > 0.0) ? bi.inflated(pad) : bi;

    bool conflict = false;
    for (size_t j = 0; j < i; j++)
    {
      if (!kept[j])
        continue;
      const auto& bj = chosen[j];
      const LabelBBox test_j = (pad > 0.0) ? bj.inflated(pad) : bj;

      // i's label covering j's marker, or j's already-kept label
      // covering i's marker — either way drop i.
      const auto& ob_j = candidates[j].obstacle;
      const auto& ob_i = candidates[i].obstacle;
      if (ob_j.valid() && test_i.overlaps(ob_j))
      {
        conflict = true;
        break;
      }
      if (ob_i.valid() && test_j.overlaps(ob_i))
      {
        conflict = true;
        break;
      }
    }
    kept[i] = !conflict;
  }

  std::vector<PlacedLabel> result;
  result.reserve(n);
  for (size_t i = 0; i < n; i++)
  {
    if (kept[i])
      result.push_back(makePlaced(candidates[i], chosen[i]));
    else
      result.push_back(makeDropped(candidates[i]));
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
  const size_t n = candidates.size();
  std::vector<PlacedLabel> result;
  result.reserve(n);

  // Flat list of placed bounding boxes for O(n·k·m) overlap check.
  // For typical weather-map label counts (<200) this is fast enough.
  std::vector<LabelBBox> placed_bboxes;
  placed_bboxes.reserve(n);

  const double pad = config.bbox_padding;
  const bool fs_on = config.free_space_weight > 0.0;
  const double fs_radius = config.free_space_radius;
  const double fs_radius2 = fs_radius * fs_radius;

  // Centroids of labels already placed in this run.  Greedy is one-pass
  // in priority order, so when candidate i is being placed, every label
  // in this list belongs to a higher-priority candidate (j < i).  Adding
  // their centroids to the free-direction sum lets each new label see
  // the previously-placed labels as occupied space, not just markers.
  // (Without this, the free direction is computed from anchor points
  // alone and a wide neighbour-label can still end up under the new one
  // even when the markers are technically far apart.)
  std::vector<std::pair<double, double>> placed_centroids;
  placed_centroids.reserve(n);

  auto computeFreeDir = [&](size_t i) -> std::pair<double, double> {
    if (!fs_on)
      return {0.0, 0.0};
    double fx = 0.0;
    double fy = 0.0;
    auto accumulate = [&](double ox, double oy) {
      const double dx = candidates[i].anchor_x - ox;
      const double dy = candidates[i].anchor_y - oy;
      const double d2 = dx * dx + dy * dy;
      if (d2 < 1e-9)
        return;
      if (fs_radius > 0.0 && d2 > fs_radius2)
        return;
      fx += dx / d2;
      fy += dy / d2;
    };
    for (size_t j = 0; j < n; j++)
      if (j != i)
        accumulate(candidates[j].anchor_x, candidates[j].anchor_y);
    for (const auto& c : placed_centroids)
      accumulate(c.first, c.second);
    const double mag = std::sqrt(fx * fx + fy * fy);
    if (mag <= 1e-9)
      return {0.0, 0.0};
    return {fx / mag, fy / mag};
  };

  for (size_t i = 0; i < n; i++)
  {
    const auto& cand = candidates[i];
    const auto [sym_half_w, sym_half_h] = candidateSymbolHalf(cand);
    auto bboxes = candidateBBoxes(cand.anchor_x, cand.anchor_y,
                                  cand.label_w, cand.label_h,
                                  config.offset, config.candidates,
                                  sym_half_w, sym_half_h);
    const std::pair<double, double> free_dir = computeFreeDir(i);
    const std::vector<int> order =
        orderedPositionIndices(static_cast<int>(bboxes.size()), free_dir,
                               config.free_space_weight);
    bool placed = false;
    for (int oi : order)
    {
      const auto& bbox = bboxes[static_cast<size_t>(oi)];
      if (!withinBounds(bbox, map_w, map_h))
        continue;

      // Inflate for collision testing only — absorbs sub-pixel font-metric
      // variation between systems and gives a small visual breathing space.
      const LabelBBox test = (pad > 0.0) ? bbox.inflated(pad) : bbox;

      bool overlap = false;
      for (const auto& pb : placed_bboxes)
      {
        if (test.overlaps(pb))
        {
          overlap = true;
          break;
        }
      }
      if (!overlap)
      {
        // Test against every other candidate's marker, but never
        // against the label's own marker — the candidate position
        // was deliberately laid out to clear it, and applying padding
        // there would push the label uselessly far from its own
        // anchor.
        for (size_t j = 0; j < n; j++)
        {
          if (j == i)
            continue;
          const auto& ob = candidates[j].obstacle;
          if (!ob.valid())
            continue;
          if (test.overlaps(ob))
          {
            overlap = true;
            break;
          }
        }
      }
      if (!overlap)
      {
        result.push_back(makePlaced(cand, bbox));
        placed_bboxes.push_back(test);
        if (fs_on)
          placed_centroids.push_back(
              {(bbox.x1 + bbox.x2) / 2.0, (bbox.y1 + bbox.y2) / 2.0});
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
// PRIORITY-GREEDY — re-sorts by population descending, runs greedy,
// then restores input order so callers can correlate result[i] with
// input[i] (e.g. to suppress the marker for a dropped label).
// ------------------------------------------------------------------
std::vector<PlacedLabel> priorityGreedyPlacement(const LabelConfig& config,
                                                  std::vector<LabelCandidate> candidates,
                                                  double map_w, double map_h)
{
  const size_t n = candidates.size();
  std::vector<size_t> order(n);
  for (size_t i = 0; i < n; i++)
    order[i] = i;

  // stable_sort preserves geonames order for equal populations
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) {
                     return candidates[a].population > candidates[b].population;
                   });

  std::vector<LabelCandidate> sorted;
  sorted.reserve(n);
  for (auto idx : order)
    sorted.push_back(candidates[idx]);

  auto sorted_result = greedyCore(config, sorted, map_w, map_h);

  std::vector<PlacedLabel> result(n);
  for (size_t i = 0; i < n; i++)
    result[order[i]] = sorted_result[i];
  return result;
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

  const double pad = config.bbox_padding;

  // Pre-compute all candidate bounding boxes for every label.  Two
  // versions are kept: render_bboxes are the natural (un-padded) boxes
  // returned to the caller; coll_bboxes are inflated by pad for collision
  // detection so platform-specific font measurement noise can't flip
  // close decisions.
  std::vector<std::vector<LabelBBox>> render_bboxes(static_cast<size_t>(n));
  std::vector<std::vector<LabelBBox>> coll_bboxes(static_cast<size_t>(n));
  for (int i = 0; i < n; i++)
  {
    const auto& ci = candidates[static_cast<size_t>(i)];
    const auto [sym_half_w, sym_half_h] = candidateSymbolHalf(ci);
    render_bboxes[static_cast<size_t>(i)] =
        candidateBBoxes(ci.anchor_x, ci.anchor_y,
                        ci.label_w, ci.label_h,
                        config.offset,
                        config.candidates,
                        sym_half_w, sym_half_h);
    auto& dst = coll_bboxes[static_cast<size_t>(i)];
    dst.reserve(render_bboxes[static_cast<size_t>(i)].size());
    for (const auto& b : render_bboxes[static_cast<size_t>(i)])
      dst.push_back(pad > 0.0 ? b.inflated(pad) : b);
  }

  // Marker obstacles indexed parallel to candidates so we can skip self
  // when computing the obstacle-overlap energy.  Invalid bboxes (zero
  // size) are kept in the list and ignored at lookup time.
  std::vector<LabelBBox> obstacles(static_cast<size_t>(n));
  for (int i = 0; i < n; i++)
    obstacles[static_cast<size_t>(i)] = candidates[static_cast<size_t>(i)].obstacle;

  // assignment[i]: index into render_bboxes[i], or -1 if dropped
  std::vector<int> assignment(static_cast<size_t>(n), -1);

  // Initialise: each label at its first within-bounds candidate.  Bounds
  // are checked against the natural (render) bbox; padding is for
  // overlap detection only and does not constrain map bounds.
  for (int i = 0; i < n; i++)
  {
    for (int k = 0; k < static_cast<int>(render_bboxes[static_cast<size_t>(i)].size()); k++)
    {
      if (withinBounds(render_bboxes[static_cast<size_t>(i)][static_cast<size_t>(k)], map_w, map_h))
      {
        assignment[static_cast<size_t>(i)] = k;
        break;
      }
    }
  }

  // Delta-energy contributions for label i at candidate position k.
  // Splits into label-vs-label and label-vs-obstacle terms because
  // obstacles are static (don't depend on assignment) and weighted
  // separately to make symbol-overlap a hard penalty.
  auto labelOverlapContrib = [&](int i, int k) -> double {
    if (k < 0)
      return 0.0;
    const auto& bi = coll_bboxes[static_cast<size_t>(i)][static_cast<size_t>(k)];
    double e = 0.0;
    for (int j = 0; j < n; j++)
    {
      if (j == i)
        continue;
      const int kj = assignment[static_cast<size_t>(j)];
      if (kj < 0)
        continue;
      e += bi.overlap_area(coll_bboxes[static_cast<size_t>(j)][static_cast<size_t>(kj)]);
    }
    return e;
  };

  auto obstacleOverlapContrib = [&](int i, int k) -> double {
    if (k < 0)
      return 0.0;
    const auto& bi = coll_bboxes[static_cast<size_t>(i)][static_cast<size_t>(k)];
    double e = 0.0;
    for (int j = 0; j < n; j++)
    {
      if (j == i)
        continue;  // never test a label against its own marker
      const auto& ob = obstacles[static_cast<size_t>(j)];
      if (!ob.valid())
        continue;
      e += bi.overlap_area(ob);
    }
    return e;
  };

  // Position penalty: dropped label costs (num_candidates) to discourage dropping
  const int num_cands = static_cast<int>(
      render_bboxes.empty() ? 0 : render_bboxes[0].size());

  // Free-space bias: when enabled, the position penalty is reduced for
  // candidate positions whose direction aligns with the candidate's
  // free-direction vector — same idea as the greedy reorder, expressed
  // here as an extra penalty term so SA balances it against the overlap
  // and base position-index costs.
  //
  // Free direction is recomputed for the candidate being moved on each
  // posPenalty call.  It includes both other candidates' markers AND
  // their currently-assigned label centroids, so a wide neighbour-label
  // is treated as occupied space.  Cost: O(n) per call ≈ 200 ops, run
  // ~4 × sa_iterations times — well under a millisecond at typical
  // settings.
  const double fw = config.free_space_weight;
  const double fs_radius = config.free_space_radius;
  const double fs_radius2 = fs_radius * fs_radius;

  auto computeFreeDir = [&](int i) -> std::pair<double, double> {
    if (fw <= 0.0)
      return {0.0, 0.0};
    double fx = 0.0;
    double fy = 0.0;
    auto accumulate = [&](double ox, double oy) {
      const double dx = candidates[static_cast<size_t>(i)].anchor_x - ox;
      const double dy = candidates[static_cast<size_t>(i)].anchor_y - oy;
      const double d2 = dx * dx + dy * dy;
      if (d2 < 1e-9)
        return;
      if (fs_radius > 0.0 && d2 > fs_radius2)
        return;
      fx += dx / d2;
      fy += dy / d2;
    };
    for (int j = 0; j < n; j++)
    {
      if (j == i)
        continue;
      const auto& cj = candidates[static_cast<size_t>(j)];
      accumulate(cj.anchor_x, cj.anchor_y);
      const int kj = assignment[static_cast<size_t>(j)];
      if (kj >= 0)
      {
        const auto& bj = render_bboxes[static_cast<size_t>(j)][static_cast<size_t>(kj)];
        accumulate((bj.x1 + bj.x2) / 2.0, (bj.y1 + bj.y2) / 2.0);
      }
    }
    const double mag = std::sqrt(fx * fx + fy * fy);
    if (mag <= 1e-9)
      return {0.0, 0.0};
    return {fx / mag, fy / mag};
  };

  auto posPenalty = [&](int i, int k) -> double {
    if (k < 0)
      return static_cast<double>(num_cands);
    double penalty = static_cast<double>(k);
    if (fw > 0.0)
    {
      const auto fd = computeFreeDir(i);
      if (fd.first != 0.0 || fd.second != 0.0)
      {
        const auto& r = positionRule(num_cands, k);
        const double alignment = r.dx * fd.first + r.dy * fd.second;
        penalty -= fw * alignment;
      }
    }
    return penalty;
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
    const auto& bboxes_i = render_bboxes[static_cast<size_t>(i)];
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

    // Obstacle overlap reuses the label-overlap weight so a marker
    // collision is treated at least as harshly as a label collision.
    const double dE =
        ow * (labelOverlapContrib(i, new_k) - labelOverlapContrib(i, old_k)) +
        ow * (obstacleOverlapContrib(i, new_k) - obstacleOverlapContrib(i, old_k)) +
        pw * (posPenalty(i, new_k) - posPenalty(i, old_k));

    if (dE < 0.0 || prob_dist(rng) < std::exp(-dE / std::max(T, 1e-10)))
      assignment[static_cast<size_t>(i)] = new_k;

    T *= cool;
  }

  // Build result vector — return natural (render) bboxes, not the
  // padded collision bboxes used internally.
  std::vector<PlacedLabel> result;
  result.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; i++)
  {
    const int k = assignment[static_cast<size_t>(i)];
    if (k >= 0)
      result.push_back(
          makePlaced(candidates[static_cast<size_t>(i)],
                     render_bboxes[static_cast<size_t>(i)][static_cast<size_t>(k)]));
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
