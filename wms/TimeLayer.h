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
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  boost::optional<std::string> timezone;

  std::vector<std::string> timestamp;  // these two should be of equal size
  std::vector<std::string> format;
  std::string formatter = "boost";

  std::string prefix;  // These are usually needed only for time duration formatting
  std::string suffix;

  // This may be negative and can override attributes:
  boost::optional<int> x;
  boost::optional<int> y;
  boost::optional<double> longitude;  // longitude, latitude take precedence over x, y
  boost::optional<double> latitude;

  // We need a parameter definition in order to find a correct generation / origintime
  // (grid-support)
  /*
  boost::optional<std::string> parameter;
*/
 private:
  virtual void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  virtual void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  boost::posix_time::ptime now;

};  // class TimeLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
