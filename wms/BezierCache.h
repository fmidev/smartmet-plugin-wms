// ======================================================================
/*!
 * \brief Cache for bezier-fitted polylines.
 *
 * Adjacent isobands share rings: one's exterior edge is the next one's
 * (reversed) hole edge. Without a shared cache, each isoband fits the
 * shared edge independently and tiny floating-point differences leave
 * visible gaps between the bands.
 *
 * The cache stores fitted cubics keyed by a canonical-direction hash of
 * the polyline points. Two callers that present the same polyline (in
 * either direction) get bit-identical cubics — the second caller skips
 * fitting entirely.
 *
 * The cache is also used for isolines, since an isoline at value V
 * coincides with the V-boundary edges of the surrounding isobands and
 * is frequently rendered on top of them.
 */
// ======================================================================

#pragma once

#include "BezierFit.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class BezierCache
{
 public:
  // True if the polyline is in canonical direction (first point < last
  // point lexicographically). Empty and 1-point polylines are canonical.
  static bool isCanonical(const std::vector<Fmi::BezierFit::Point>& pts);

  // Hash a polyline's point sequence. Caller must ensure canonical
  // direction before hashing — different directions yield different
  // hashes.
  static std::size_t hashCanonical(const std::vector<Fmi::BezierFit::Point>& pts);

  // Result of canonicalizing a closed ring. Two rings tracing the same
  // loop produce the same `canonical` (rotation + direction invariant),
  // but `callerForward` records which direction the caller traversed it
  // so the cubics can be reversed back to caller's direction on cache
  // hit (preserving winding-fill semantics).
  struct CanonicalRing
  {
    std::vector<Fmi::BezierFit::Point> canonical;
    bool callerForward = true;
  };

  // Build the canonical form of a closed ring [v_0, ..., v_{n-1}, v_0]
  // (with closing duplicate). The canonical form is rotated so the
  // lex-smallest vertex is at index 0, with the smaller of its two
  // neighbours next, and the closing duplicate appended at the end.
  // callerForward is true iff the caller's traversal direction agrees
  // with the canonical direction.
  static CanonicalRing canonicalizeClosedRing(
      const std::vector<Fmi::BezierFit::Point>& pts);

  // Look up cached cubics for the given canonical-direction hash.
  // Returns nullptr on miss.
  const std::vector<Fmi::BezierFit::CubicBez>* find(std::size_t key) const;

  // Store cubics under the given canonical-direction hash.
  void insert(std::size_t key, std::vector<Fmi::BezierFit::CubicBez> cubics);

  std::size_t size() const { return m_cache.size(); }

  // Hit/miss counters for diagnostics. Updated by find/insert.
  std::size_t hits() const { return m_hits; }
  std::size_t misses() const { return m_misses; }

 private:
  std::unordered_map<std::size_t, std::vector<Fmi::BezierFit::CubicBez>> m_cache;
  mutable std::size_t m_hits = 0;
  mutable std::size_t m_misses = 0;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
