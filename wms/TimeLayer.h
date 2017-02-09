// ======================================================================
/*!
 * \brief Time layer
 */
// ======================================================================

#pragma once

#include "Layer.h"

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
  boost::optional<std::string> timestamp;
  boost::optional<std::string> format;

  // This may be negative and can override attributes:
  boost::optional<int> x;
  boost::optional<int> y;

 private:
  boost::posix_time::ptime now;

};  // class TimeLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
