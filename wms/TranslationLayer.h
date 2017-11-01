// ======================================================================
/*!
 * \brief A SVG text element with translations
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
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

class TranslationLayer : public Layer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theParent);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  std::string tag = "text";
  std::map<std::string, std::string> translations;

 private:
};  // class TranslationLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
