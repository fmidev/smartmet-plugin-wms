#include "Base64.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
std::string base64_encode(const unsigned char* theData, std::size_t theSize)
{
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string out;
  out.resize(((theSize + 2) / 3) * 4);
  char* o = out.data();

  std::size_t i = 0;
  for (; i + 3 <= theSize; i += 3)
  {
    const unsigned int n = (static_cast<unsigned int>(theData[i]) << 16) |
                           (static_cast<unsigned int>(theData[i + 1]) << 8) |
                           static_cast<unsigned int>(theData[i + 2]);
    *o++ = table[(n >> 18) & 0x3f];
    *o++ = table[(n >> 12) & 0x3f];
    *o++ = table[(n >> 6) & 0x3f];
    *o++ = table[n & 0x3f];
  }

  const std::size_t rest = theSize - i;
  if (rest == 1)
  {
    const unsigned int n = static_cast<unsigned int>(theData[i]) << 16;
    *o++ = table[(n >> 18) & 0x3f];
    *o++ = table[(n >> 12) & 0x3f];
    *o++ = '=';
    *o++ = '=';
  }
  else if (rest == 2)
  {
    const unsigned int n = (static_cast<unsigned int>(theData[i]) << 16) |
                           (static_cast<unsigned int>(theData[i + 1]) << 8);
    *o++ = table[(n >> 18) & 0x3f];
    *o++ = table[(n >> 12) & 0x3f];
    *o++ = table[(n >> 6) & 0x3f];
    *o++ = '=';
  }

  return out;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
