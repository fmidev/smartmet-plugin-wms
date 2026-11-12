// ======================================================================
/*!
 * \brief Filters
 *
 *  container
 */
// ======================================================================

#pragma once

#include <json/forwards.h>
#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
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
