// ======================================================================
/*!
 * \brief Satellite image layer
 *
 * Renders precoloured satellite imagery served by the satellite engine.
 * Unlike the other data sources the images are ready for display: the
 * pixels are RGBA and no styling is applied, hence the projection
 * transformation uses nearest neighbour interpolation.
 *
 * The producer is the satellite and the parameter is the composite of
 * it, which lets a client list the satellites and then the composites of
 * the one the user picked:
 *
 *   { "layer_type": "satellite", "producer": "meteosat", "parameter": "natural" }
 *
 * Some products are not precoloured but hold values, for example a cloud
 * top temperature in Kelvin. Those need a colour map, which is given the
 * same way as for a raster layer:
 *
 *   { "layer_type": "satellite", "producer": "meteosat",
 *     "parameter": "ctth_tempe", "colormap": "cloud_top_temperature" }
 *
 * The image closest to the requested time is used. The tolerance is
 * configurable, since a geostationary satellite produces an image every
 * fifteen minutes while a polar orbiter passes only a few times a day.
 */
// ======================================================================

#pragma once

#include "ColorMap.h"
#include "Layer.h"
#include <engines/satellite/Engine.h>
#include <macgyver/DateTime.h>
#include <macgyver/TimeParser.h>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class SatelliteLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  // Maximum distance from the requested time to the time of the image.
  // A geostationary satellite produces an image every fifteen minutes,
  // a polar orbiter passes only a few times a day.
  Fmi::TimeDuration time_tolerance{Fmi::Hours(1)};

  // PNG compression level of the image embedded into the SVG
  int compression{1};

  // Uncoloured products need a colour map. The name refers to a colour
  // map resource of the customer, the same ones the raster layer uses.
  std::string colormap_name;
  bool smooth_colors{true};

 private:
  const Engine::Satellite::Engine& getEngine(const State& theState) const;

  Engine::Satellite::ImageInfoPtr findImage(const State& theState) const;

  // Precoloured images are used as they are, uncoloured ones are given
  // their colours here
  Engine::Satellite::Image renderPixels(const State& theState,
                                        const Engine::Satellite::ImageInfo& theImage,
                                        const Engine::Satellite::WarpOptions& theOptions) const;

  std::shared_ptr<ColorMap> colormap;

  // The rendered image, reused over the animation loop steps of one
  // frame. Time animation changes the valid time between the frames,
  // hence the identity of the image the cached pixels came from must be
  // remembered as well.
  std::string svg_image;
  std::size_t svg_image_hash{0};

};  // class SatelliteLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
