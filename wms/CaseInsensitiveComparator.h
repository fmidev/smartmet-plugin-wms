// ======================================================================
/*!
 * \brief A Meb Maps Service (WMS) Request
 */
// ======================================================================

#pragma once
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct CaseInsensitiveComparator : std::binary_function<std::string, std::string, bool>
{
  static char asciilower(char ch)
  {
    char ret = ch;
    if (ch >= 'A' && ch <= 'Z')
      ret = static_cast<char>(ch + ('a' - 'A'));
    return ret;
  }

  bool operator()(const std::string& first, const std::string& second) const
  {
    std::size_t n = std::min(first.size(), second.size());
    for (std::size_t i = 0; i < n; i++)
    {
      char ch1 = asciilower(first[i]);
      char ch2 = asciilower(second[i]);
      if (ch1 != ch2)
        return false;
    }

    return (first.size() == second.size());
  }
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
