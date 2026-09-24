#include "NamespacePattern.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{
// Namespace regexes select layer name prefixes, they have no need to be long.
// Limiting the length limits the cost of pathological (backtracking) patterns.
const std::size_t max_pattern_length = 100;

bool looks_like_pattern(const std::string& s)
{
  return s.size() >= 2 && boost::algorithm::starts_with(s, "/") &&
         boost::algorithm::ends_with(s, "/");
}

const boost::regex& compiled_pattern(const std::string& re_str)
{
  // GetCapabilities matches the same pattern against every layer name
  thread_local std::string cached_str;
  thread_local boost::regex cached_re;
  thread_local bool cached = false;

  if (!cached || re_str != cached_str)
  {
    if (re_str.size() > max_pattern_length)
      throw Fmi::Exception(BCP, "Namespace pattern is too long")
          .addParameter("Maximum length", std::to_string(max_pattern_length));
    cached_re = boost::regex(re_str, boost::regex::icase);
    cached_str = re_str;
    cached = true;
  }
  return cached_re;
}

}  // namespace

bool match_namespace_pattern(const std::string& name, const std::string& pattern)
{
  try
  {
    if (name == pattern)
      return true;

    if (!looks_like_pattern(pattern))
      return boost::algorithm::istarts_with(name, pattern + ":");

    // Strip surrounding slashes first
    const std::string re_str = pattern.substr(1, pattern.size() - 2);
    return boost::regex_search(name, compiled_pattern(re_str));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Namespace pattern matching failed")
        .addParameter("Pattern", pattern);
  }
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
