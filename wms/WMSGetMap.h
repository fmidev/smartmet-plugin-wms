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
// struct for map info
struct tag_map_info
{
  std::string name;  // name
  std::string style;

  tag_map_info(const std::string& n, const std::string& s) : name(n), style(s) {}
};

struct tag_get_map_request_options
{
  std::vector<tag_map_info> map_info_vector;

  Spine::BoundingBox bbox;  // contains crs
  std::string format;
  std::string version;
  // optional
  bool transparent;
  struct tab_bgcolor
  {
    unsigned int red;
    unsigned int green;
    unsigned int blue;
  } bgcolor;
  std::string exceptions;
  std::vector<boost::posix_time::ptime> timesteps;
  int elevation;
  unsigned int width;
  unsigned int height;
  bool debug;
};

class WMSGetMap
{
 public:
  WMSGetMap(const WMSConfig& theConfig);

  void parseHTTPRequest(const Engine::Querydata::Engine& theQEngine,
                        Spine::HTTP::Request& theRequest);
  std::string jsonText() const;

 private:
  tag_get_map_request_options itsParameters;

  const WMSConfig& itsConfig;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
