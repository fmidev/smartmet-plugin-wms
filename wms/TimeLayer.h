// ======================================================================
/*!
 * \brief Time layer
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class TimeLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theParent);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  boost::optional<std::string> timezone;

  std::vector<std::string> timestamp;  // these two should be of equal size
  std::vector<std::string> format;

  std::string prefix;  // These are usually needed only for time duration formatting
  std::string suffix;

  // This may be negative and can override attributes:
  boost::optional<int> x;
  boost::optional<int> y;
  boost::optional<int> longitude;  // longitude, latitude take precedence over x, y
  boost::optional<int> latitude;

 private:
  boost::posix_time::ptime now;

};  // class TimeLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
