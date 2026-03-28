#pragma once

#include "Config.h"
#include <engines/querydata/Engine.h>
#include <macgyver/TemplateFactory.h>
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
namespace GetCapabilities
{
// response to GetCapabilities request
std::string response(const Fmi::SharedFormatter& theFormatter,
                     const Spine::HTTP::Request& theRequest,
                     const Engine::Querydata::Engine& theQEngine,
                     const Config& theConfig);

}  // namespace GetCapabilities

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
