// ======================================================================
/*!
 * \brief Hash calculation utilities
 */
// ======================================================================

#pragma once
#include "State.h"
#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>
#include <macgyver/StringConversion.h>
#include <list>
#include <map>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
inline void hash_combine(std::size_t& hash1, std::size_t hash2)
{
  if (hash1 == 0 || hash2 == 0)
    hash1 = 0;
  else
    boost::hash_combine(hash1, hash2);
}

// Times
inline std::size_t hash_value(const boost::posix_time::ptime& time)
{
  return boost::hash_value(Fmi::to_iso_string(time));
}

// Normal objects

template <typename T>
inline std::size_t hash_value(const T& obj)
{
  return boost::hash_value(obj);
}

// Optional objects with a normal hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::optional<T>& obj)
{
  if (!obj)
    return boost::hash_value(false);
  else
  {
    std::size_t hash = Dali::hash_value(*obj);
    if (hash != 0)
      boost::hash_combine(hash, boost::hash_value(true));
    return hash;
  }
}

// Objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const T& obj, const State& theState)
{
  return obj.hash_value(theState);
}

// Optional objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::optional<T>& obj, const State& theState)
{
  if (!obj)
    return boost::hash_value(false);
  else
  {
    std::size_t hash = obj->hash_value(theState);
    if (hash == 0)
      return 0;
    boost::hash_combine(hash, boost::hash_value(true));
    return hash;
  }
}

// Shared objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::shared_ptr<T>& obj, const State& theState)
{
  if (!obj)
    return boost::hash_value(false);
  else
  {
    std::size_t hash = Dali::hash_value(*obj, theState);
    if (hash == 0)
      return 0;
    boost::hash_combine(hash, boost::hash_value(true));
    return hash;
  }
}

// Vectors of Dali objects
template <typename T>
inline std::size_t hash_value(const std::vector<T>& objs, const State& theState)
{
  std::size_t hash = 0;
  for (const auto& obj : objs)
  {
    std::size_t subhash = Dali::hash_value(obj, theState);
    if (subhash == 0)
      return 0;
    boost::hash_combine(hash, subhash);
  }
  return hash;
}

// Lists of Dali objects
template <typename T>
inline std::size_t hash_value(const std::list<T>& objs, const State& theState)
{
  std::size_t hash = 0;
  for (const auto& obj : objs)
  {
    std::size_t subhash = Dali::hash_value(obj, theState);
    if (subhash == 0)
      return 0;
    boost::hash_combine(hash, subhash);
  }
  return hash;
}

// Maps
template <typename T, typename S>
inline std::size_t hash_value(const std::map<T, S>& objs)
{
  std::size_t hash = 0;
  for (const auto& obj : objs)
  {
    boost::hash_combine(hash, Dali::hash_value(obj.first));
    boost::hash_combine(hash, Dali::hash_value(obj.second));
  }
  return hash;
}

// Symbols. Unfortunately we cannot just use getSymbol since
// the symbol may be defined inline in the json. Also, we're
// not checking markers, filters, patterns or gradients either,
// and they could all be inline too...
inline std::size_t hash_symbol(const std::string& name, const State& theState)
{
  std::size_t hash = boost::hash_value(name);
  boost::hash_combine(hash, theState.getSymbolHash(name));
  return hash;
  // Slower option:
  // return boost::hash_value(theState.getSymbol(name));
}

inline std::size_t hash_symbol(const boost::optional<std::string>& name, const State& theState)
{
  if (!name)
    return boost::hash_value(false);
  else
  {
    auto hash = boost::hash_value(true);
    boost::hash_combine(hash, hash_symbol(*name, theState));
    return hash;
  }
}

// Style sheets
inline std::size_t hash_css(const std::string& name, const State& theState)
{
  return boost::hash_value(theState.getStyle(name));
}

inline std::size_t hash_css(const boost::optional<std::string>& name, const State& theState)
{
  if (!name)
    return boost::hash_value(false);
  else
  {
    auto hash = boost::hash_value(true);
    boost::hash_combine(hash, hash_css(*name, theState));
    return hash;
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
