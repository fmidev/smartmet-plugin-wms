#pragma once
#include "PointHash.h"
#include <unordered_map>
#include <unordered_set>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class VertexCounter
{
 public:
  void add(const OGRGeometry* geometry);
  int getCount(const OGRPoint& point) const;

 private:
  std::unordered_map<OGRPoint, int, OGRPointHash, OGRPointEqual> pointCountMap;
  void processGeometry(const OGRGeometry* geometry);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
