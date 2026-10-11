#pragma once

#include <spine/HTTP.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
// The parts of the self-referential URLs written into capabilities documents and
// landing pages. They come from client-controlled headers and parameters, so each one
// is validated before it is spliced into already-rendered XML or JSON.

// "http://" or "https://" from X-Forwarded-Proto, anything else gives "http://"
std::string requestProtocol(const Spine::HTTP::Request& theRequest);

// The Host header without a legacy trailing "/wms", or the fallback when the header is
// missing or is not a plain hostname[:port] or [IPv6][:port]
std::string requestHost(const Spine::HTTP::Request& theRequest, const std::string& theFallback);

// "/fmi-apikey/<key>" when the request carries an apikey that may be returned to the
// client and consists of URL-safe characters only, otherwise an empty string
std::string requestApiKeyPath(const Spine::HTTP::Request& theRequest,
                              bool theCheckAccessToken = false);

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
