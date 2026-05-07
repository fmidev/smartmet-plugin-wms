#include "BezierCache.h"

#include <macgyver/Hash.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

bool BezierCache::isCanonical(const std::vector<Fmi::BezierFit::Point>& pts)
{
  if (pts.size() < 2)
    return true;
  const auto& a = pts.front();
  const auto& b = pts.back();
  if (a.x != b.x)
    return a.x < b.x;
  if (a.y != b.y)
    return a.y < b.y;
  // Endpoints coincide (closed sub-segment); fall back to comparing the
  // second vertex so the direction is still deterministic.
  if (pts.size() >= 3)
  {
    const auto& a2 = pts[1];
    const auto& b2 = pts[pts.size() - 2];
    if (a2.x != b2.x)
      return a2.x < b2.x;
    if (a2.y != b2.y)
      return a2.y < b2.y;
  }
  return true;
}

std::size_t BezierCache::hashCanonical(const std::vector<Fmi::BezierFit::Point>& pts)
{
  std::size_t h = pts.size();
  for (const auto& p : pts)
  {
    Fmi::hash_combine(h, Fmi::hash_value(p.x));
    Fmi::hash_combine(h, Fmi::hash_value(p.y));
  }
  return h;
}

namespace
{
bool ptLess(const Fmi::BezierFit::Point& a, const Fmi::BezierFit::Point& b)
{
  if (a.x != b.x)
    return a.x < b.x;
  return a.y < b.y;
}
}  // namespace

BezierCache::CanonicalRing BezierCache::canonicalizeClosedRing(
    const std::vector<Fmi::BezierFit::Point>& pts)
{
  // Caller passes the closed ring with the closing duplicate (v_0
  // repeated at the end). The canonical form is independent of the
  // ring's traversal start and direction, so two rings tracing the same
  // loop produce identical results. callerForward records whether the
  // caller's traversal matched the canonical direction so cached cubics
  // can be reversed back to the caller's winding on a cache hit.
  CanonicalRing result;
  if (pts.size() < 3)
  {
    result.canonical = pts;
    return result;
  }
  const std::size_t n = pts.size() - 1;  // strip closing duplicate

  // Find the index of the lex-smallest vertex (deterministic anchor).
  std::size_t k = 0;
  for (std::size_t i = 1; i < n; i++)
  {
    if (ptLess(pts[i], pts[k]))
      k = i;
  }

  // Decide canonical direction: the canonical "next" vertex is whichever
  // neighbour of v_k is lex-smaller. callerForward is true iff the
  // caller's "next" (i.e. pts[k+1]) matches the canonical "next".
  const std::size_t nxt = (k + 1) % n;
  const std::size_t prv = (k + n - 1) % n;
  result.callerForward = ptLess(pts[nxt], pts[prv]);

  result.canonical.reserve(pts.size());
  if (result.callerForward)
  {
    for (std::size_t i = 0; i < n; i++)
      result.canonical.push_back(pts[(k + i) % n]);
  }
  else
  {
    for (std::size_t i = 0; i < n; i++)
      result.canonical.push_back(pts[(k + n - i) % n]);
  }
  // Re-append the closing duplicate so the polyline is closed.
  result.canonical.push_back(result.canonical.front());
  return result;
}

const std::vector<Fmi::BezierFit::CubicBez>* BezierCache::find(std::size_t key) const
{
  auto it = m_cache.find(key);
  if (it == m_cache.end())
  {
    ++m_misses;
    return nullptr;
  }
  ++m_hits;
  return &it->second;
}

void BezierCache::insert(std::size_t key, std::vector<Fmi::BezierFit::CubicBez> cubics)
{
  m_cache.emplace(key, std::move(cubics));
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
