#pragma once

#include <string>
#include <map>
#include <set>

#include <engines/querydata/Engine.h>
#include <spine/Value.h>
#include <spine/HTTP.h>

#include "TemplateFactory.h"
#include "WMSConfig.h"

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSGetCapabilities
{
 public:
  WMSGetCapabilities(const std::string& theTemplatePath);

  // response to GetCapabilities request
  std::string response(const SmartMet::Spine::HTTP::Request& theRequest,
                       const SmartMet::Engine::Querydata::Engine& theQEngine,
                       const WMSConfig& theConfig) const;

 private:
  Dali::SharedFormatter itsResponseFormatter;

  std::string resolveGetMapURI(const SmartMet::Spine::HTTP::Request& theRequest) const;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
