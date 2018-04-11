// ======================================================================
/*!
 * \brief Filters
 *
 *  container
 */
// ======================================================================

#pragma once

#include <json/json.h>

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

class Filters
{
 private:
  typedef std::map<std::string, std::string> FilterMap;
  FilterMap filters;

 public:
  void init(const Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
