//======================================================================

#include "SatelliteLayer.h"
#include "Base64.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include <boost/algorithm/string/join.hpp>
#include <algorithm>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <gis/Box.h>
#include <giza/Giza.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <spine/Json.h>
#include <cmath>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void SatelliteLayer::init(Json::Value& theJson,
                          const State& theState,
                          const Config& theConfig,
                          const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Satellite layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    std::optional<Fmi::TimeDuration> tolerance;
    JsonTools::remove_duration(tolerance, theJson, "time_tolerance");
    if (tolerance)
    {
      if (tolerance->total_seconds() < 0)
        throw Fmi::Exception(BCP, "Satellite layer time_tolerance must not be negative");
      time_tolerance = *tolerance;
    }

    JsonTools::remove_int(compression, theJson, "compression");

    JsonTools::remove_string(colormap_name, theJson, "colormap");
    JsonTools::remove_bool(smooth_colors, theJson, "smooth_colors");

    // The producer is the satellite and the parameter is the composite
    if (!paraminfo.producer)
      throw Fmi::Exception(BCP, "Satellite layer requires a producer");

    if (paraminfo.parameter.empty())
      throw Fmi::Exception(BCP, "Satellite layer requires a parameter")
          .addParameter("Producer", *paraminfo.producer);

    const auto& engine = getEngine(theState);

    if (!engine.hasProducer(*paraminfo.producer))
      throw Fmi::Exception(BCP, "Unknown satellite producer")
          .addParameter("Producer", *paraminfo.producer);

    if (!engine.hasProduct(*paraminfo.producer, paraminfo.parameter))
      throw Fmi::Exception(BCP, "Unknown parameter for this satellite producer")
          .addParameter("Producer", *paraminfo.producer)
          .addParameter("Parameter", paraminfo.parameter)
          .addParameter("Available parameters",
                        boost::algorithm::join(engine.parameters(*paraminfo.producer), ","));

    if (!colormap_name.empty())
    {
      std::string cmap = theState.getColorMap(colormap_name);
      if (cmap.empty())
        throw Fmi::Exception(BCP, "Cannot find the colormap")
            .addParameter("colormap", colormap_name);
      colormap_hash = Fmi::hash(cmap);
      colormap = std::make_shared<ColorMap>(cmap);
    }

    // Whether a colour map is needed depends on the data, not on the
    // configuration, so check it against the newest image. Reading the
    // metadata of the images is what the engine does at scan time, hence
    // this costs nothing.
    auto newest = engine.find(*paraminfo.producer, paraminfo.parameter, {}, time_tolerance);

    if (newest)
    {
      const bool uncoloured = (newest->model == Engine::Satellite::BandModel::Float);

      if (uncoloured && !colormap)
        throw Fmi::Exception(BCP, "The satellite product holds values and needs a colormap")
            .addParameter("Producer", *paraminfo.producer)
            .addParameter("Parameter", paraminfo.parameter);

      if (!uncoloured && colormap)
        throw Fmi::Exception(
            BCP, "The satellite product is precoloured and a colormap would have no effect")
            .addParameter("Producer", *paraminfo.producer)
            .addParameter("Parameter", paraminfo.parameter)
            .addParameter("colormap", colormap_name);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief How long the response for this image may be cached
 *
 * An image is never rewritten, so a request which pins the time gets
 * the same pixels for as long as the file exists, and the response can
 * be cached for a long time; the ETag makes revalidation cheap in any
 * case. The exception is the newest image: a request without a time
 * resolves to it, and the answer to that request changes when the next
 * image arrives. Such responses expire when the next image is due,
 * estimated from the interval of the two newest ones, so that a client
 * animating the latest imagery notices new frames without polling the
 * server for every tile in between.
 */
// ----------------------------------------------------------------------

Fmi::DateTime SatelliteLayer::expirationTime(const State& theState,
                                             const Engine::Satellite::ImageInfo& theImage) const
{
  try
  {
    const auto now = Fmi::SecondClock::universal_time();
    const auto& engine = getEngine(theState);

    const auto times = engine.times(*paraminfo.producer, paraminfo.parameter);
    if (times.empty() || theImage.time < times.back())
      return now + Fmi::Hours(24);

    // The newest image. Expect the next one after the usual interval,
    // bounded so that a gap in the data or a single stray image does not
    // produce an unreasonable estimate.
    auto interval = Fmi::Minutes(15);
    if (times.size() >= 2)
      interval = times.back() - times[times.size() - 2];
    interval =
        std::clamp(interval, Fmi::TimeDuration(Fmi::Minutes(1)), Fmi::TimeDuration(Fmi::Hours(3)));

    const auto due = times.back() + interval;
    return std::max(due, now + Fmi::Minutes(1));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Access the satellite engine
 */
// ----------------------------------------------------------------------

const Engine::Satellite::Engine& SatelliteLayer::getEngine(const State& theState) const
{
  const auto* engine = theState.getSatelliteEngine();

  if (engine == nullptr)
    throw Fmi::Exception(BCP,
                         "Satellite layers are not available: the satellite engine is not loaded");

  return *engine;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find the image to be rendered
 *
 * Returns nullptr if there is no suitable image. This is not an error:
 * a tile may well be requested for a time the satellite has no image
 * for.
 */
// ----------------------------------------------------------------------

Engine::Satellite::ImageInfoPtr SatelliteLayer::findImage(const State& theState) const
{
  try
  {
    const auto& engine = getEngine(theState);

    std::optional<Fmi::DateTime> time;
    if (hasValidTime())
      time = getValidTime();

    return engine.find(*paraminfo.producer, paraminfo.parameter, time, time_tolerance);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Produce the pixels of the requested area
 *
 * A precoloured image is warped as it is. An uncoloured one is warped as
 * values and coloured here with the colour map, which is the same
 * machinery the raster layer uses for model data.
 */
// ----------------------------------------------------------------------

Engine::Satellite::Image SatelliteLayer::renderPixels(
    const State& theState,
    const Engine::Satellite::ImageInfo& theImage,
    const Engine::Satellite::WarpOptions& theOptions) const
{
  try
  {
    const auto& engine = getEngine(theState);

    if (theImage.model != Engine::Satellite::BandModel::Float)
      return engine.warp(theImage, theOptions);

    if (!colormap)
      throw Fmi::Exception(BCP, "The satellite product holds values and needs a colormap");

    auto values = engine.warpValues(theImage, theOptions);

    Engine::Satellite::Image image;
    image.width = values.width;
    image.height = values.height;
    image.pixels.resize(values.values.size(), 0);

    for (std::size_t i = 0; i < values.values.size(); i++)
    {
      const auto value = values.values[i];

      // Missing values stay transparent. Note that zero is a perfectly
      // good temperature, so the check cannot be against zero.
      if (std::isnan(value))
        continue;

      image.pixels[i] = colormap->getColor(value, smooth_colors);
    }

    return image;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("Path", theImage.path);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Warp the image and add it to the product
 */
// ----------------------------------------------------------------------

void SatelliteLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    auto image = findImage(theState);
    if (!image)
      return;  // No image for this time: draw nothing

    theState.updateModificationTime(image->time);
    theState.updateExpirationTime(expirationTime(theState, *image));

    const auto& box = projection.getBox();

    if (!qid.empty())
      theState.requireId(qid);
    else
      qid = theState.makeQid("satellite");

    bool vis = true;
    if (theState.animation_enabled)
    {
      vis = isAnimationStepVisible(theState);
      if (!vis)
        svg_image.clear();
    }

    // Time animation renders the same layer object several times with a
    // different valid time, hence the cached pixels can be reused only
    // when they came from the same image
    if (svg_image_hash != image->hash)
      svg_image.clear();

    if (visible && vis && svg_image.empty())
    {
      Engine::Satellite::WarpOptions options;
      options.crs = projection.getCRS().WKT();
      options.bbox = {box.xmin(), box.ymin(), box.xmax(), box.ymax()};
      options.width = static_cast<int>(box.width());
      options.height = static_cast<int>(box.height());

      auto warped = renderPixels(theState, *image, options);

      int comp = compression;
      if (theState.animation_enabled)
        comp = 1;

      // The image is precoloured, hence the pixels are encoded as they are
      const auto png = Giza::topng_argb(warped.pixels.data(), warped.width, warped.height, comp);

      svg_image = fmt::format(
          "<image id=\"{}\" href=\"data:image/png;base64,{}\" x=\"0\" y=\"0\" width=\"{}\" "
          "height=\"{}\" />\n\n",
          qid,
          Dali::base64_encode(png),
          warped.width,
          warped.height);
      svg_image_hash = image->hash;
    }

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    std::ostringstream useOut;
    useOut << "<use xlink:href=\"#" << qid << "\"/>\n";

    if (visible && !svg_image.empty())
      theGlobals["includes"][qid] = svg_image;

    theGlobals["bbox"] = Fmi::to_string(box.xmin()) + "," + Fmi::to_string(box.ymin()) + "," +
                         Fmi::to_string(box.xmax()) + "," + Fmi::to_string(box.ymax());

    group_cdt["cdata"] = useOut.str();
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!")
        .addParameter("qid", qid)
        .addParameter("Producer", paraminfo.producer ? *paraminfo.producer : std::string("-"))
        .addParameter("Parameter", paraminfo.parameter);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The pixel values of a precoloured image carry no information
 *        which could be reported to the user
 */
// ----------------------------------------------------------------------

void SatelliteLayer::getFeatureInfo(CTPP::CDT& theInfo, const State& theState) {}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value of the layer
 *
 * The image is identified by the hash the engine calculated from the
 * file name, size and modification time. Hashing the pixels would be
 * both slow and pointless: the files are never modified in place.
 */
// ----------------------------------------------------------------------

std::size_t SatelliteLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    Fmi::hash_combine(hash, Fmi::hash_value(time_tolerance));
    Fmi::hash_combine(hash, Fmi::hash_value(compression));
    Fmi::hash_combine(hash, Fmi::hash_value(colormap_name));
    Fmi::hash_combine(hash, Fmi::hash_value(smooth_colors));
    Fmi::hash_combine(hash, colormap_hash);

    auto image = findImage(theState);

    // Without an image the product renders nothing, which is a perfectly
    // cacheable result
    if (!image)
      Fmi::hash_combine(hash, Fmi::hash_value(std::string("no image")));
    else
      Fmi::hash_combine(hash, image->hash);

    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "SatelliteLayer::hash_value failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
