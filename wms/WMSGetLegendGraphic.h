#pragma once

#include "WMSConfig.h"
#include "WMSGetCapabilities.h"

#include <ctpp2/CDT.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

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
  unsigned int width;
  unsigned int height;
};

class WMSGetLegendGraphic
{
 public:
  WMSGetLegendGraphic(const WMSConfig& theConfig);

  void parseHTTPRequest(const Engine::Querydata::Engine& theQEngine,
                        Spine::HTTP::Request& theRequest);

  std::string jsonText() const;

 private:
  get_legend_graphic_request_options itsParameters;

  const WMSConfig& itsConfig;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
