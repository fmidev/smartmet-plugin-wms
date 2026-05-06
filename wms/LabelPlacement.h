// ======================================================================
/*!
 * \brief Point-feature label placement for LocationLayer
 *
 * Implements several algorithms from the cartographic literature for
 * placing text labels next to point symbols without mutual overlap.
 * All placement is performed in pixel space after map projection.
 *
 * The module is intentionally dependency-free (no SmartMet/jsoncpp
 * headers) so that the algorithms can be unit-tested in isolation.
 * JSON parsing of LabelConfig is handled in LocationLayer.cpp.
 *
 * Weather data is the primary information on the map; location labels
 * exist only to identify where weather data is located.  The caller
 * is responsible for ordering SVG layers so that weather layers render
 * on top of the location layer.
 *
 * Supported algorithms
 * --------------------
 * none            : no labels rendered (default, backward-compatible)
 * fixed           : label at one fixed Imhof position, no conflict check
 * greedy          : try k candidate positions, first non-overlapping wins;
 *                   input order provides priority (geonames sort order)
 * priority-greedy : same as greedy but re-sorted by population descending
 * simulated-annealing : Christensen-Marks-Shieber (1995) global optimiser
 *
 * References
 * ----------
 * Imhof, E. (1975). Positioning names on maps.
 *   The American Cartographer, 2(2), 128-144.
 * Christensen, J., Marks, J., Shieber, S. (1995). An empirical study of
 *   algorithms for point-feature label placement.
 *   ACM Transactions on Graphics, 14(3), 203-232.
 */
// ======================================================================

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// -----------------------------------------------------------------------
// Axis-aligned bounding box in pixel coordinates (y increases downward)
// -----------------------------------------------------------------------
struct LabelBBox
{
  double x1 = 0, y1 = 0, x2 = 0, y2 = 0;

  bool valid() const noexcept { return x2 > x1 && y2 > y1; }
  bool overlaps(const LabelBBox& o) const noexcept;
  double overlap_area(const LabelBBox& o) const noexcept;

  // Inflated copy: each edge moved outward by pad
  LabelBBox inflated(double pad) const noexcept
  {
    return {x1 - pad, y1 - pad, x2 + pad, y2 + pad};
  }
};

// -----------------------------------------------------------------------
// Input to placement algorithms (one per location that passed mindistance)
// Text dimensions must be pre-measured at the effective font size.
// -----------------------------------------------------------------------
struct LabelCandidate
{
  double anchor_x = 0;  // pixel position of the map symbol
  double anchor_y = 0;
  std::string text;     // location name
  double population = 0;
  unsigned int label_w = 0;  // pre-measured pixel width
  unsigned int label_h = 0;  // pre-measured pixel height
  // Cairo y_bearing — the offset from the SVG <text> baseline to the
  // top of the visual glyph extent.  Typically negative.  Threading
  // this through placement to render lets emit code align the visual
  // glyphs with bbox.y1 instead of with the baseline.
  int y_bearing = 0;

  // Effective style (resolved from LabelConfig + population classes)
  double font_size = 11;
  std::string font_weight = "normal";
  std::string fill = "#333333";

  // Marker obstacle bounding box centered on the anchor (symbol size +
  // optional margin).  Default-constructed (zero-sized) means no obstacle.
  LabelBBox obstacle;
};

// -----------------------------------------------------------------------
// Output: one entry per input candidate; placed=false means dropped
// -----------------------------------------------------------------------
struct PlacedLabel
{
  LabelBBox bbox;        // bounding box of placed text
  std::string text;
  double population = 0;

  // Resolved style (copied from LabelCandidate for rendering)
  double font_size = 11;
  std::string font_weight = "normal";
  std::string fill = "#333333";

  // y_bearing forwarded from LabelCandidate so the renderer can place
  // the SVG <text> baseline such that the visual glyphs line up with
  // bbox.y1 .. bbox.y2 (not offset by descent).
  int y_bearing = 0;

  bool placed = false;
};

// -----------------------------------------------------------------------
// Algorithm choice
// -----------------------------------------------------------------------
enum class PlacementAlgorithm
{
  None,              // no labels
  Fixed,             // single fixed offset, no conflict detection
  Greedy,            // greedy in geonames priority order
  PriorityGreedy,    // greedy after descending-population sort
  SimulatedAnnealing // Christensen-Marks-Shieber global optimiser
};

// -----------------------------------------------------------------------
// Population-dependent style override
// -----------------------------------------------------------------------
struct LabelStyleClass
{
  std::optional<double> lolimit;   // inclusive lower population bound
  std::optional<double> hilimit;   // exclusive upper population bound
  double font_size = 0;            // 0 = inherit from LabelConfig default
  std::string font_weight;         // "" = inherit
  std::string fill;                // "" = inherit
  // Marker obstacle overrides; 0 = inherit from LabelConfig default.
  // symbol_size is the shorthand alias for symmetric markers; explicit
  // width/height take precedence when both are given.
  double symbol_width = 0;
  double symbol_height = 0;

  bool matches(double population) const noexcept;
};

// -----------------------------------------------------------------------
// Full label configuration.
// Parsed from the "label": { ... } JSON block in LocationLayer.
// Constructed directly (no init() method) to keep this header free of
// SmartMet/jsoncpp dependencies.  Parsing lives in LocationLayer.cpp.
// -----------------------------------------------------------------------
struct LabelConfig
{
  PlacementAlgorithm algorithm = PlacementAlgorithm::None;

  // Candidate geometry
  int candidates = 8;    // how many candidate positions to try: 4, 8, or 16
  double offset = 5.0;   // pixel gap between symbol edge and label

  // Marker obstacle: a bbox centered on the anchor.  Treated as a reserved
  // region during placement.  Markers are assumed centered on the anchor
  // with overflow=visible — caller passes the rendered marker size.
  // Defaults are 8x8 — a small dot-style marker — which gives sensible
  // behaviour for typical weather/location maps without configuration.
  // Set to zero to disable the obstacle entirely.
  double symbol_width = 8.0;
  double symbol_height = 8.0;
  double symbol_margin = 0.0;   // extra margin around the symbol

  // Cross-platform stability for placement: collision detection inflates
  // each label bbox by bbox_padding pixels on every side, and label
  // dimensions are quantised up to a multiple of bbox_quantum before
  // placement.  Together these absorb 1-2 px font-metric variation
  // between distros (e.g. Rocky10 vs RHEL9 fontconfig differences).
  // bbox_quantum = 0 disables quantisation.  Defaults are zero so
  // existing tests are unaffected.
  double bbox_padding = 0.0;
  unsigned int bbox_quantum = 0;

  // Default text style
  std::string font_family = "sans-serif";
  double font_size = 11;
  std::string font_weight = "normal";
  std::string fill = "#333333";
  std::string stroke = "white";    // halo colour; empty = no halo
  double stroke_width = 2.0;
  double stroke_opacity = 0.75;
  int max_labels = 200;            // cap on input candidates (after mindistance)

  // fixed algorithm: one of n|ne|e|se|s|sw|w|nw
  std::string fixed_position = "ne";

  // simulated-annealing parameters
  int sa_iterations = 2000;
  double sa_initial_temp = 1.0;
  double sa_cooling_rate = 0.99;
  double sa_overlap_weight = 1.0;   // penalty weight for overlapping label area
  double sa_position_weight = 0.2;  // penalty for using a less-preferred position

  // Free-space bias: when > 0, each candidate's position priority is
  // shifted toward the direction that points away from neighbouring
  // markers, weighted by inverse-square distance.  This pushes coastal
  // labels into the sea (or away from inland clusters) without needing
  // a sea mask — the only "occupied" regions known to the algorithm
  // are the other markers themselves.  free_space_radius caps the
  // neighbourhood (0 = use all candidates regardless of distance).
  // Greedy: positions are sorted by (position_index − weight × alignment)
  // so highly-aligned positions are tried first.  SA: the same term is
  // added to the position penalty.
  double free_space_weight = 0.0;
  double free_space_radius = 0.0;

  // Population-dependent style overrides (first match wins)
  std::vector<LabelStyleClass> classes;

  bool empty() const noexcept { return algorithm == PlacementAlgorithm::None; }

  // Resolve effective style for a given population value
  double effective_font_size(double population) const noexcept;
  std::string effective_font_weight(double population) const;
  std::string effective_fill(double population) const;
  double effective_symbol_width(double population) const noexcept;
  double effective_symbol_height(double population) const noexcept;
};

// -----------------------------------------------------------------------
// Geometry helper exposed for unit testing
//
// Returns up to num_candidates bounding boxes around (ax, ay) for a label
// of pixel size (w, h), in Imhof preference order (best position first).
// num_candidates is clamped to 4, 8, or 16.
//
// symbol_half_w / symbol_half_h push the label outward from the anchor
// so it clears the marker rectangle.  Pass zero when no marker obstacle.
// -----------------------------------------------------------------------
std::vector<LabelBBox> candidateBBoxes(double ax,
                                       double ay,
                                       unsigned int w,
                                       unsigned int h,
                                       double offset,
                                       int num_candidates,
                                       double symbol_half_w = 0.0,
                                       double symbol_half_h = 0.0);

// -----------------------------------------------------------------------
// Main placement function
//
// Applies the algorithm specified in config to the given candidates and
// returns one PlacedLabel per input (placed=false for dropped labels).
// map_width / map_height are used as hard bounds: no label may extend
// outside [0, map_width] x [0, map_height].
// -----------------------------------------------------------------------
std::vector<PlacedLabel> placeLabels(const LabelConfig& config,
                                     std::vector<LabelCandidate> candidates,
                                     double map_width,
                                     double map_height);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
