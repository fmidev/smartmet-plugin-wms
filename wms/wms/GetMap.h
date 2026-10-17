#pragma once

#include "Config.h"
#include "GetCapabilities.h"
#include <ctpp2/CDT.hpp>
#include <macgyver/DateTime.h>
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
  std::string name;
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
  bool transparent = false;
  struct tab_bgcolor
  {
    unsigned int red = 0;
    unsigned int green = 0;
    unsigned int blue = 0;
  } bgcolor;
  std::string exceptions;
  std::optional<Fmi::DateTime> reference_time;
  std::vector<Fmi::DateTime> timesteps;
  std::optional<int> elevation;
  unsigned int width = 0;
  unsigned int height = 0;
  std::optional<int> interval_start;
  std::optional<int> interval_end;

  bool debug = false;
};

class GetMap
{
 public:
  explicit GetMap(const Config& theConfig);

  void parseHTTPRequest(const Engine::Querydata::Engine& theQEngine,
                        Spine::HTTP::Request& theRequest);

  std::vector<Json::Value> jsons() const;
  std::vector<std::string> styles() const;

 private:
  std::string validateLayers(const std::vector<std::string>& layers) const;
  void validateStyles(const std::vector<std::string>& layers,
                      std::vector<std::string>& styles,
                      const std::string& styles_opt) const;
  void parseImageSize(const Spine::HTTP::Request& theRequest);
  void parseColors(const Spine::HTTP::Request& theRequest);
  void parseReferenceTime(Spine::HTTP::Request& theRequest, const std::string& layer_origintime);
  void parseTimes(const Spine::HTTP::Request& theRequest);
  void parseIntervals(const Spine::HTTP::Request& theRequest);
  void addIntervalParameters(Spine::HTTP::Request& theRequest, const std::string& layerName) const;
  void resolveTime(Spine::HTTP::Request& theRequest, const std::string& layerName) const;

  tag_get_map_request_options itsParameters;
  std::string itsFilename;

  const Config& itsConfig;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
