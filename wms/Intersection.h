// ======================================================================
/*!
 * \brief Intersection definition
 */
// ======================================================================

#pragma once

#include "Smoother.h"
#include <gis/Types.h>
#include <json/json.h>
#include <macgyver/DateTime.h>
#include <cstddef>
#include <optional>
#include <string>

namespace SmartMet::Engine::Grid
{
class Engine;
}  // namespace SmartMet::Engine::Grid
namespace SmartMet::Plugin::Dali
{
class Config;
}  // namespace SmartMet::Plugin::Dali

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Projection;
class State;

class Intersection
{
 public:
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

  OGRGeometryPtr intersect(OGRGeometryPtr theGeometry) const;
  bool inside(double theX, double theY) const;

  // One or both may be missing. Interpretation:
  // null,value --> -inf,value
  // value,null --> value,inf
  // null,null  --> missing values

  std::optional<double> lolimit;
  std::optional<double> hilimit;
  std::optional<double> value;  // special case used for observations

  std::optional<double> level;
  std::optional<std::string> producer;
  std::optional<std::string> parameter;

  std::string interpolation{"linear"};

  Smoother smoother;

  std::string unit_conversion;
  std::optional<double> multiplier;
  std::optional<double> offset;

  // Note: No inside/outside here, use them on the Layer level instead

 private:
  OGRGeometryPtr isoband;

};  // class Intersection

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
