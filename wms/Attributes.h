// ======================================================================
/*!
 * \brief Generic SVG element attribute container
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <json/json.h>
#include <map>
#include <set>
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

class Attributes
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  void add(const std::string& theName, const std::string& theValue);
  void add(const Attributes& theAttributes);

  void generate(CTPP::CDT& theLocals, State& theState) const;
  void generatePresentation(CTPP::CDT& theLocals,
                            State& theState,
                            const std::string& theCSS = {}) const;
  std::size_t hash_value(const State& theState) const;

  boost::optional<std::string> getLocalIri(const std::string& theName) const;

  bool empty() const { return attributes.empty(); }
  std::string value(const std::string& theName) const
  {
    if (attributes.find(theName) == attributes.end())
      return "";

    return attributes.at(theName);
  }

  boost::optional<std::string> remove(const std::string& theName);

 private:
  std::map<std::string, std::string> attributes;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
