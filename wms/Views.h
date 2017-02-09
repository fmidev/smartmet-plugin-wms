// ======================================================================
/*!
 * \brief Views container
 */
// ======================================================================

#pragma once

#include "View.h"
#include <json/json.h>
#include <list>
#include <boost/shared_ptr.hpp>

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

 private:
  std::list<boost::shared_ptr<View> > views;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
