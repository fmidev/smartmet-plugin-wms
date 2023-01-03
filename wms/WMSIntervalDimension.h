// ======================================================================
/*!
 * \brief A Web Map Service interval dimension data structure
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

/*
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/Exception.h>

#include <list>
#include <set>
*/

#include <boost/optional.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct interval_dimension_item
{
  interval_dimension_item(int s, int e, bool d)
      : interval_start(s), interval_end(e), interval_default(d)
  {
  }
  int interval_start;
  int interval_end;
  bool interval_default{false};
};

class WMSIntervalDimension
{
 public:
  virtual ~WMSIntervalDimension() = default;
  WMSIntervalDimension(int interval_start, int interval_end, bool interval_default);

  WMSIntervalDimension() = delete;
  WMSIntervalDimension(const WMSIntervalDimension& other) = delete;
  WMSIntervalDimension operator=(const WMSIntervalDimension& other) = delete;
  WMSIntervalDimension(WMSIntervalDimension&& other) = delete;
  WMSIntervalDimension operator=(WMSIntervalDimension&& other) = delete;

  std::string getStartIntervals() const;
  std::string getEndIntervals() const;
  std::string getDefaultStartInterval() const;
  std::string getDefaultEndInterval() const;
  void addInterval(int interval_start, int interval_end, bool interval_default);
  std::string getIntervals() const;
  std::string getDefaultInterval() const;
  bool isValidInterval(int interval_start, int interval_end) const;

 private:
  std::vector<interval_dimension_item> itsIntervals;  // Intervals back- and forwards
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
