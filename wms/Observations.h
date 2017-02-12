// ======================================================================
/*!
 * \brief Observations
 */
// ======================================================================

#pragma once

#ifndef WITHOUT_OBSERVATION

#include "Observation.h"
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

class Observations
{
 public:
  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  std::vector<Observation> observations;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
