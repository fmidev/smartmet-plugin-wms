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
}  // namespace CTPP

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
  using CSSMap = std::map<std::string, std::string>;

 public:
  CSSMap csss;
  void init(Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
