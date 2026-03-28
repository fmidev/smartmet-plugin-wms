#pragma once

#include "Config.h"
#include "GetCapabilities.h"

#include <ctpp2/CDT.hpp>

#include <spine/HTTP.h>
#include <spine/Value.h>

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
