// ======================================================================
/*!
 * \brief Views container
 */
// ======================================================================

#pragma once

#include "ParameterInfo.h"
#include "View.h"
#include <boost/shared_ptr.hpp>
#include <json/json.h>
#include <list>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace HTTP
{
class Request;
}

namespace Plugin
{
namespace Dali
{
class Config;

class Views
{
 public:
  void init(const Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties);

  void generate(CTPP::CDT& theGlobals, State& theState);

  std::size_t hash_value(const State& theState) const;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::list<boost::shared_ptr<View> > views;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
