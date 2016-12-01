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
  boost::optional<boost::posix_time::ptime> time;
  boost::optional<int> time_offset;
  Projection projection;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
