// ======================================================================
/*!
 * \brief Views container
 */
// ======================================================================

#pragma once

#include "ParameterInfo.h"
#include "View.h"
#include "Warnings.h"
#include <json/json.h>
#include <list>
#include <memory>

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

  std::size_t hash_value(const State& theState) const;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::list<std::shared_ptr<View> > views;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
