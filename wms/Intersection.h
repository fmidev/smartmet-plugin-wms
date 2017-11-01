// ======================================================================
/*!
 * \brief Intersection definition
 */
// ======================================================================

#pragma once

#include "Smoother.h"
#include <boost/optional.hpp>
#include <engines/gis/Engine.h>
#include <engines/querydata/Engine.h>
#include <json/json.h>
#include <string>

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

  void init(Engine::Querydata::Q q,
            const Projection& theProjection,
            const boost::posix_time::ptime& theTime,
            const State& theState);

  std::size_t hash_value(const State& theState) const;

  OGRGeometryPtr intersect(OGRGeometryPtr theGeometry) const;
  bool inside(double theX, double theY) const;

  // One or both may be missing. Interpretation:
  // null,value --> -inf,value
  // value,null --> value,inf
  // null,null  --> missing values

  boost::optional<double> lolimit;
  boost::optional<double> hilimit;
  boost::optional<double> value;  // special case used for observations

  boost::optional<double> level;
  boost::optional<std::string> producer;
  boost::optional<std::string> parameter;

  std::string interpolation{"linear"};

  Smoother smoother;

  boost::optional<double> multiplier;
  boost::optional<double> offset;

  // Note: No inside/outside here, use them on the Layer level instead

 private:
  OGRGeometryPtr isoband;

};  // class Intersection

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
