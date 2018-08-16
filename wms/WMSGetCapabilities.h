#pragma once

#include "TemplateFactory.h"
#include "WMSConfig.h"
#include <engines/querydata/Engine.h>
#include <spine/HTTP.h>
#include <spine/Value.h>
#include <map>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace WMSGetCapabilities
{
// response to GetCapabilities request
std::string response(const Dali::SharedFormatter& theFormatter,
                     const Spine::HTTP::Request& theRequest,
                     const Engine::Querydata::Engine& theQEngine,
                     const WMSConfig& theConfig);

}  // namespace WMSGetCapabilities

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
