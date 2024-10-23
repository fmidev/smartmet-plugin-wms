// ----------------------------------------------------------------------
/*!
 * \brief Inheritable properties
 */
// ----------------------------------------------------------------------

#pragma once

#include "Projection.h"

#include <gis/Box.h>
#include <macgyver/DateTime.h>
#include <optional>

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

  void init(Json::Value& theJson, const State& theState, const Config& theConfig);

  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  bool hasValidTime() const;
  void setValidTime(const Fmi::DateTime& theTime);
  Fmi::DateTime getValidTime() const;
  Fmi::DateTime getValidTime(const Fmi::DateTime& theDefault) const;
  Fmi::TimePeriod getValidTimePeriod() const;
  bool inside(const Fmi::Box& theBox, double theX, double theY) const;

  std::size_t hash_value(const State& theState) const;

  std::optional<std::string> language;
  std::optional<std::string> producer;
  std::optional<std::string> source;
  std::optional<int> forecastType;
  std::optional<int> forecastNumber;
  std::optional<uint> geometryId;
  std::optional<double> level;
  std::optional<std::string> elevation_unit;
  std::optional<double> pressure;
  std::optional<int> levelId;
  std::optional<int> timestep;

  // Wanted origintime. Use latest if omitted
  std::optional<Fmi::DateTime> origintime;

  // Timezone for time parsing
  Fmi::TimeZonePtr tz;

  // An interval can be used to select the latest observation available, if any, or for example all
  // lightning observations. For forecasts the interval settings are meaningless.

  std::optional<int> interval_start;  // minutes
  std::optional<int> interval_end;    // minutes

  Projection projection;

  // Clipping margins
  int xmargin = 0;
  int ymargin = 0;
  bool clip = false;

 private:
  // Wanted time T = time + time_offset. Making these private forces using getValidTime()
  std::optional<Fmi::DateTime> time;
  std::optional<int> time_offset;  // minutes
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
