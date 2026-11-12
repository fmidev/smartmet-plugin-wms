// ======================================================================
/*!
 * \brief Markers
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

class Markers
{
 private:
  using MarkerMap = std::map<std::string, std::string>;
  MarkerMap markers;

 public:
  void init(Json::Value& theJson, const State& theState);
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
