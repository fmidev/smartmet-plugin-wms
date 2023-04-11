// ======================================================================
/*!
 * \brief Smoother details
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Smoother
{
 public:
  void init(Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  boost::optional<double> size;
  boost::optional<double> degree;

 private:
};  // class Smoother

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
