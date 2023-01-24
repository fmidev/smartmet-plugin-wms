// ----------------------------------------------------------------------
/*!
 * \brief Inheritable properties
 */
// ----------------------------------------------------------------------

#pragma once

#include "Projection.h"

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/optional.hpp>
#include <gis/Box.h>

#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;
class TimeLayer;

class Properties
{
 public:
  friend TimeLayer;  // for more fine grained control

  virtual ~Properties() = default;

  Properties() = default;
  Properties(const Properties& other) = delete;
  Properties(const Properties&& other) = delete;
  Properties& operator=(const Properties& other) = delete;
  Properties& operator=(const Properties&& other) = delete;

  void init(const Json::Value& theJson, const State& theState, const Config& theConfig);

  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  bool hasValidTime() const;
  void setValidTime(const boost::posix_time::ptime& theTime);
  boost::posix_time::ptime getValidTime() const;
  boost::posix_time::ptime getValidTime(const boost::posix_time::ptime& theDefault) const;
  boost::posix_time::time_period getValidTimePeriod() const;
  bool inside(const Fmi::Box& theBox, double theX, double theY) const;

  std::size_t hash_value(const State& theState) const;

  boost::optional<std::string> language;
  boost::optional<std::string> producer;
  boost::optional<std::string> source;
  boost::optional<int> forecastType;
  boost::optional<int> forecastNumber;
  boost::optional<uint> geometryId;
  boost::optional<double> level;
  boost::optional<int> levelId;
  boost::optional<int> timestep;

  // Wanted origintime. Use latest if omitted
  boost::optional<boost::posix_time::ptime> origintime;

  // Timezone for time parsing
  boost::local_time::time_zone_ptr tz;

  // An interval can be used to select the latest observation available, if any, or for example all
  // lightning observations. For forecasts the interval settings are meaningless.

  boost::optional<int> interval_start;  // minutes
  boost::optional<int> interval_end;    // minutes

  Projection projection;

  // Clipping margins
  int xmargin = 0;
  int ymargin = 0;
  bool clip = false;

 private:
  // Wanted time T = time + time_offset. Making these private forces using getValidTime()
  boost::optional<boost::posix_time::ptime> time;
  boost::optional<int> time_offset;  // minutes
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
