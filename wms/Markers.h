// ======================================================================
/*!
 * \brief Markers
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

class Markers
{
 private:
  typedef std::map<std::string, std::string> MarkerMap;
  MarkerMap markers;

 public:
  void init(const Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
