// ======================================================================
/*!
 * \brief Views container
 */
// ======================================================================

#pragma once

#include "ParameterInfo.h"
#include <json/forwards.h>
#include <cstddef>
#include <list>
#include <memory>

namespace SmartMet::Plugin::Dali
{
class Properties;
class State;
class View;
struct Warnings;
}  // namespace SmartMet::Plugin::Dali

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

class Views
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  void check_warnings(Warnings& warnings) const;

  void generate(CTPP::CDT& theGlobals, State& theState);
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState);

  std::size_t hash_value(const State& theState) const;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::list<std::shared_ptr<View> > views;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
