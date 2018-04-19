// ======================================================================
/*!
 * \brief CSSs
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

class CSSs
{
 private:
  typedef std::map<std::string, std::string> CSSMap;

 public:
  CSSMap csss;
  void init(const Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
