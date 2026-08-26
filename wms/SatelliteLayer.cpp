//======================================================================

#include "SatelliteLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include <boost/algorithm/string/join.hpp>
#include <ctpp2/CDT.hpp>
#include <gis/Box.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <spine/Json.h>

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

      auto warped = getEngine(theState).warp(*image, options);

      int comp = compression;
      if (theState.animation_enabled)
        comp = 1;

      // The image is precoloured, hence the pixels can be encoded as they are
      const int size = warped.width * warped.height;
      const int buffersize = size * 4 + 10000;
      std::vector<char> buffer(buffersize);

      const int bytes = png_saveMem(
          buffer.data(), buffersize, warped.pixels.data(), warped.width, warped.height, comp);

      if (bytes <= 0)
        throw Fmi::Exception(BCP, "Failed to encode the satellite image as PNG");

      std::ostringstream svgImage;
      svgImage << "<image id=\"" << qid << "\" href=\"data:image/png;base64,";
      svgImage << base64_encode(reinterpret_cast<unsigned char*>(buffer.data()), bytes);
      svgImage << "\" x=\"0\" y=\"0\" width=\"" << warped.width << "\" height=\"" << warped.height
               << "\" />\n\n";

      svg_image = svgImage.str();
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
