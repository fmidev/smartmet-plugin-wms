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

#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
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

class IntervalDimension
{
 public:
  virtual ~IntervalDimension();
  IntervalDimension(int interval_start, int interval_end, bool interval_default);

  IntervalDimension() = delete;
  IntervalDimension(const IntervalDimension& other) = delete;
  IntervalDimension operator=(const IntervalDimension& other) = delete;
  IntervalDimension(IntervalDimension&& other) = delete;
  IntervalDimension operator=(IntervalDimension&& other) = delete;

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

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
