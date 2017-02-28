// ----------------------------------------------------------------------
/*!
 * \brief Inheritable properties
 */
// ----------------------------------------------------------------------

#pragma once
#include "Projection.h"
#include <boost/optional.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

struct Properties
{
  virtual ~Properties() {}
  void init(const Json::Value& theJson, const State& theState, const Config& theConfig);

  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  std::size_t hash_value(const State& theState) const;

  boost::optional<std::string> language;
  boost::optional<std::string> producer;

  // Wanted time T = time + time_offset
  boost::optional<boost::posix_time::ptime> time;
  boost::optional<int> time_offset;  // minutes

  // Unless we want an interval, for example for lightning data, then we use
  // interval T-interval_start ... T+interval_end. Also, some observations
  // come at irregular intervals. An interval can be used to select the
  // latest observation available, if any.

  boost::optional<int> interval_start;  // minutes
  boost::optional<int> interval_end;    // minutes

  Projection projection;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
