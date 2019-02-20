// ======================================================================
/*!
 * \brief Isoband details
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Text.h"
#include <boost/optional.hpp>
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Isoband
{
 public:
  std::string getQid(const State& theState) const;

  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  // One or both may be missing. Interpretation:
  // null,value --> -inf,value
  // value,null --> value,inf
  // null,null  --> missing values

  std::string qid;
  boost::optional<double> lolimit;
  boost::optional<double> hilimit;

  // SVG attributes (id, class, style, ...)
  Attributes attributes;

  boost::optional<Text> label;

 private:
  mutable std::string generated_qid;

};  // class Isoband

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
