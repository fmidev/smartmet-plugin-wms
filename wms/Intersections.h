// ======================================================================
/*!
 * \brief Intersections cut an input geometry with contoured isobands
 */
// ======================================================================

#pragma once

#include "Intersection.h"
#include <gis/Types.h>
#include <json/json.h>
#include <macgyver/DateTime.h>
#include <cstddef>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet::Engine::Grid
{
class Engine;
}  // namespace SmartMet::Engine::Grid
namespace SmartMet::Plugin::Dali
{
class Projection;
class State;
}  // namespace SmartMet::Plugin::Dali

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class Intersections
{
 public:
  // parameter name --> value
  using IntersectValues = std::map<std::string, double>;

  void init(const Json::Value& theJson, const Config& theConfig);

  void init(const std::optional<std::string>& theProducer,
            const Projection& theProjection,
            const Fmi::DateTime& theTime,
            const State& theState);

  void init(const std::optional<std::string>& theProducer,
            const Engine::Grid::Engine* gridEngine,
            const Projection& theProjection,
            const Fmi::DateTime& theTime,
            const State& theState);

  std::size_t hash_value(const State& theState) const;

  OGRGeometryPtr intersect(OGRGeometryPtr geom) const;
  bool inside(double theX, double theY) const;
  bool inside(const IntersectValues& theValues) const;

  std::vector<std::string> parameters() const;

 private:
  std::list<Intersection> intersections;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
