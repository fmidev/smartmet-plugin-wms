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
