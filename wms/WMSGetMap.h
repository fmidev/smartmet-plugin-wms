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

  tag_map_info(std::string n, std::string s) : name(std::move(n)), style(std::move(s)) {}
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
  boost::optional<boost::posix_time::ptime> reference_time;
  std::vector<boost::posix_time::ptime> timesteps;
  boost::optional<int> elevation;
  unsigned int width;
  unsigned int height;
  boost::optional<int> interval_start;
  boost::optional<int> interval_end;

  bool debug;
};

class WMSGetMap
{
 public:
  WMSGetMap(const WMSConfig& theConfig);

  void parseHTTPRequest(const Engine::Querydata::Engine& theQEngine,
                        Spine::HTTP::Request& theRequest);

  std::vector<Json::Value> jsons() const;
  std::vector<std::string> styles() const;

 private:
  tag_get_map_request_options itsParameters;

  const WMSConfig& itsConfig;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
