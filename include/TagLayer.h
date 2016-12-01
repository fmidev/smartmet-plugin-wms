// ======================================================================
/*!
 * \brief A SVG graphics element layer
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include "Attributes.h"
#include "Text.h"
#include <boost/optional.hpp>
#include <string>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class TagLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  boost::optional<std::string> tag;

 private:
  boost::optional<Text> cdata;

};  // class TagLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
