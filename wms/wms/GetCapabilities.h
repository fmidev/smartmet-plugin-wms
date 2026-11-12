#pragma once

#include <macgyver/TemplateFactory.h>
#include <string>

namespace SmartMet::Engine::Querydata
{
class Engine;
}  // namespace SmartMet::Engine::Querydata
namespace SmartMet::Plugin::WMS
{
class Config;
}  // namespace SmartMet::Plugin::WMS
namespace SmartMet::Spine::HTTP
{
class Request;
}  // namespace SmartMet::Spine::HTTP

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
