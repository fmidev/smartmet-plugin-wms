#pragma once
#include <ogr_geometry.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class OGRPointHash
{
 public:
  std::size_t operator()(const OGRPoint& point) const
  {
    return std::hash<double>()(point.getX()) ^ (std::hash<double>()(point.getY()) << 1);
  }
};

class OGRPointEqual
{
 public:
  bool operator()(const OGRPoint& p1, const OGRPoint& p2) const
  {
    return (p1.getX() == p2.getX()) && (p1.getY() == p2.getY());
  }
};
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet