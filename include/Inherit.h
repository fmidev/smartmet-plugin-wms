// ======================================================================
/*!
 * \brief Determine override of parameters
 *
 * Utilities for establishing the actual setting based on
 *
 *  - query string option as defined by an optional value
 *  - the value in the setting itself
 *  - the value in the parent
 *
 * Above is how things go normally if there is no default value
 * for the value itself. If there is a value, only the query string
 * option can override it, the parent never.
 *
 */
// ======================================================================

#pragma once

#include "Time.h"
#include <boost/optional.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// Prefer optional over original one

template <typename T>
inline boost::optional<T> inherit(const boost::optional<std::string>& opt_value,
                                  const boost::optional<T>& original_value)
{
  if (opt_value)
    return boost::optional<T>(boost::lexical_cast<T>(*opt_value));
  else
    return original_value;
}

// Prefer optional over original over parent
template <typename T>
inline boost::optional<T> inherit(const boost::optional<std::string>& opt_value,
                                  const boost::optional<T>& original_value,
                                  const boost::optional<T>& parent_value)
{
  if (opt_value)
    return boost::optional<T>(boost::lexical_cast<T>(*opt_value));
  else if (original_value)
    return original_value;
  else
    return parent_value;
}

// Prefer optional time over original one

template <>
inline boost::optional<boost::posix_time::ptime> inherit(
    const boost::optional<std::string>& opt_value,
    const boost::optional<boost::posix_time::ptime>& original_value)
{
  if (opt_value)
    return boost::optional<boost::posix_time::ptime>(parse_time(*opt_value));
  else
    return original_value;
}

// Prefer optional time over original over parent
template <>
inline boost::optional<boost::posix_time::ptime> inherit(
    const boost::optional<std::string>& opt_value,
    const boost::optional<boost::posix_time::ptime>& original_value,
    const boost::optional<boost::posix_time::ptime>& parent_value)
{
  if (opt_value)
    return boost::optional<boost::posix_time::ptime>(parse_time(*opt_value));
  else if (original_value)
    return original_value;
  else
    return parent_value;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
