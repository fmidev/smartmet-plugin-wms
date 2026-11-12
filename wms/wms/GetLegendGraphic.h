#pragma once

#include <json/value.h>
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
struct get_legend_graphic_request_options
{
  std::string layer;
  std::string format;
  std::string style;
  std::string version;
  std::string sld_version;
  unsigned int width = 0;
  unsigned int height = 0;
};

class GetLegendGraphic
{
 public:
  explicit GetLegendGraphic(const Config& theConfig);

  void parseHTTPRequest(const Engine::Querydata::Engine& theQEngine,
                        Spine::HTTP::Request& theRequest);

  Json::Value json() const;

 private:
  get_legend_graphic_request_options itsParameters;

  const Config& itsConfig;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
