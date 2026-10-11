#include "BaseURL.h"
#include <macgyver/Exception.h>
#include <spine/FmiApiKey.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{
bool is_alnum(char c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

// hostname[:port] or [IPv6][:port]. Rejects CR/LF, quotes, angle brackets, '&' and
// path separators, so the value cannot end an XML attribute or JSON string or redirect
// the links to another path.
bool is_valid_host(const std::string& host)
{
  if (host.empty() || host.size() > 260)
    return false;
  for (char c : host)
  {
    if (!is_alnum(c) && c != '.' && c != '-' && c != ':' && c != '[' && c != ']')
      return false;
  }
  return true;
}

// URL unreserved characters only
bool is_valid_apikey(const std::string& apikey)
{
  if (apikey.empty() || apikey.size() > 128)
    return false;
  for (char c : apikey)
  {
    if (!is_alnum(c) && c != '-' && c != '_' && c != '.' && c != '~')
      return false;
  }
  return true;
}

}  // namespace

std::string requestProtocol(const Spine::HTTP::Request& theRequest)
{
  try
  {
    auto protocol = theRequest.getProtocol();
    if (protocol && *protocol == "https")
      return "https://";
    return "http://";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string requestHost(const Spine::HTTP::Request& theRequest, const std::string& theFallback)
{
  try
  {
    auto header = theRequest.getHeader("Host");
    if (!header)
      return theFallback;

    std::string host = *header;
    if (host.size() >= 4 && host.compare(host.size() - 4, 4, "/wms") == 0)
      host.resize(host.size() - 4);

    if (!is_valid_host(host))
      return theFallback;
    return host;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string requestApiKeyPath(const Spine::HTTP::Request& theRequest, bool theCheckAccessToken)
{
  try
  {
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, theCheckAccessToken);
    if (!apikey || !Spine::FmiApiKey::shouldReturnApiKey(theRequest) || !is_valid_apikey(*apikey))
      return {};
    return "/fmi-apikey/" + *apikey;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
