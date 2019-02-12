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
// We do not use zero for empty objects or false like Boost does
const std::size_t invalid_hash = 666;

inline void hash_combine(std::size_t& hash1, std::size_t hash2)
{
  if (hash1 == invalid_hash || hash2 == invalid_hash)
    hash1 = invalid_hash;
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

// Optional objects with a normal hash_value implementation.
template <typename T>
inline std::size_t hash_value(const boost::optional<T>& obj)
{
  if (!obj)
    return Dali::hash_value(false);
  else
  {
    std::size_t hash = Dali::hash_value(*obj);
    Dali::hash_combine(hash, Dali::hash_value(true));
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
    return Dali::hash_value(false);
  else
  {
    std::size_t hash = obj->hash_value(theState);
    Dali::hash_combine(hash, boost::hash_value(true));
    return hash;
  }
}

// Shared objects with a member hash_value implementation
template <typename T>
inline std::size_t hash_value(const boost::shared_ptr<T>& obj, const State& theState)
{
  if (!obj)
    return Dali::hash_value(false);
  else
  {
    std::size_t hash = Dali::hash_value(*obj, theState);
    Dali::hash_combine(hash, boost::hash_value(true));
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
    Dali::hash_combine(hash, subhash);
    if (hash == invalid_hash)
      break;
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
    Dali::hash_combine(hash, subhash);
    if (hash == invalid_hash)
      break;
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
    Dali::hash_combine(hash, Dali::hash_value(obj.first));
    Dali::hash_combine(hash, Dali::hash_value(obj.second));
  }
  return hash;
}

// We assume the key is some simple type, and value is from Dali
template <typename T, typename S>
inline std::size_t hash_value(const std::map<T, S>& objs, const State& theState)
{
  std::size_t hash = 0;
  for (const auto& obj : objs)
  {
    Dali::hash_combine(hash, Dali::hash_value(obj.first));
    Dali::hash_combine(hash, Dali::hash_value(obj.second, theState));
  }
  return hash;
}

// Symbols. Unfortunately we cannot just use getSymbol since
// the symbol may be defined inline in the json. Also, we're
// not checking markers, filters, patterns or gradients either,
// and they could all be inline too...
inline std::size_t hash_symbol(const std::string& name, const State& theState)
{
  std::size_t hash = Dali::hash_value(name);
  Dali::hash_combine(hash, theState.getSymbolHash(name));
  return hash;
  // Slower option:
  // return boost::hash_value(theState.getSymbol(name));
}

inline std::size_t hash_symbol(const boost::optional<std::string>& name, const State& theState)
{
  if (!name)
    return Dali::hash_value(false);
  else
  {
    auto hash = Dali::hash_value(true);
    Dali::hash_combine(hash, hash_symbol(*name, theState));
    return hash;
  }
}

// Style sheets
inline std::size_t hash_css(const std::string& name, const State& theState)
{
  return Dali::hash_value(theState.getStyle(name));
}

inline std::size_t hash_css(const boost::optional<std::string>& name, const State& theState)
{
  if (!name)
    return Dali::hash_value(false);
  else
  {
    auto hash = Dali::hash_value(true);
    Dali::hash_combine(hash, hash_css(*name, theState));
    return hash;
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
