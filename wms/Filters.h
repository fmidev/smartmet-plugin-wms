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
}  // namespace CTPP

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
  using FilterMap = std::map<std::string, std::string>;
  FilterMap filters;

 public:
  void init(Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
