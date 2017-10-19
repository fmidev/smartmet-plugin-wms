// ======================================================================
/*!
 * \brief Full product specification
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Defs.h"
#include "Projection.h"
#include "Properties.h"
#include "Views.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <boost/optional.hpp>
#include <json/json.h>
#include <string>
#include <vector>

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
class State;

class Product : public Properties
{
 public:
  void init(const Json::Value& theJson, const State& theState, const Config& theConfig);

  void generate(CTPP::CDT& theGlobals, State& theState);
  std::size_t hash_value(const State& theState) const;

  // Element specific:
  boost::optional<std::string> svg_tmpl;
  std::string type;
  boost::optional<int> width;
  boost::optional<int> height;
  boost::optional<std::string> title;

  // Defs section
  Defs defs;

  // SVG attributes (id, class, style, ...)
  Attributes attributes;

  // Views
  Views views;

 private:
};  // class Product

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
