// ======================================================================
/*!
 * \brief Fast base64 encoding for images embedded into SVG
 */
// ======================================================================

#pragma once

#include <cstddef>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// Standard base64 with padding, as data URIs require. The output is built
// in one allocation with table lookups; the previously used encoder from
// grid-files went through Boost archive iterators and an ostream, which
// showed up in profiles next to the PNG encoding itself.
std::string base64_encode(const unsigned char* theData, std::size_t theSize);

inline std::string base64_encode(const std::string& theData)
{
  return base64_encode(reinterpret_cast<const unsigned char*>(theData.data()), theData.size());
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
