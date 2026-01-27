// ======================================================================

#include "Layer.h"
#include "Config.h"
#include "Defs.h"
#include "Hash.h"
#include "JsonTools.h"
#include "LayerFactory.h"
#include "State.h"
#include "View.h"
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/Cache.h>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <ogr_spatialref.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// Cache for the latlon bbox corresponding to the clipbox

const int default_cache_size = 10000;
using BBoxCache = Fmi::Cache::Cache<std::size_t, std::map<std::string, double>>;
BBoxCache g_bbox_cache(default_cache_size);

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void Layer::init(Json::Value& theJson,
                 const State& theState,
                 const Config& theConfig,
                 const Properties& theProperties)
{
  try
  {
    Properties::init(theJson, theState, theConfig, theProperties);

    JsonTools::remove_string(qid, theJson, "qid");
    JsonTools::remove_double(minresolution, theJson, "minresolution");
    JsonTools::remove_double(maxresolution, theJson, "maxresolution");

    auto json = JsonTools::remove(theJson, "enable");
    if (!json.isNull())
      JsonTools::extract_set("enable", enable, json);

    json = JsonTools::remove(theJson, "disable");
    if (!json.isNull())
      JsonTools::extract_set("disable", disable, json);

    JsonTools::remove_string(css, theJson, "css");

    json = JsonTools::remove(theJson, "attributes");
    attributes.init(json, theConfig);

    json = JsonTools::remove(theJson, "layers");
    layers.init(json, theState, theConfig, *this);

    JsonTools::remove_string(type, theJson, "layer_type");

    if (producer)
    {
      std::vector<std::string> partList;
      splitString(*producer, ':', partList);
      if (partList.size() == 2)
      {
        producer = partList[0];
        geometryId = toUInt32(partList[1]);
      }
    }

    const auto& request = theState.getRequest();

    std::optional<std::string> v = request.getParameter("geometryId");
    if (v)
      geometryId = toInt32(*v);

    // Not used in plain requests
    json = JsonTools::remove(theJson, "legend_url_layer");

    JsonTools::remove_bool(visible, theJson, "visible");
    JsonTools::remove_bool(animation_enabled, theJson, "animation_enabled");

    json = JsonTools::remove(theJson, "animation_effects");
    if (!json.isNull())
      animation_effects.init(json);

  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the layer generation is OK
 */
// ----------------------------------------------------------------------

bool Layer::validLayer(const State& theState) const
{
  try
  {
    if (!validResolution())
      return false;
    return validType(theState.getType());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the model data
 *
 * Note: It is not an error to request a model when the producer
 * refers to observations, we'll simply return an empty model instead.
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q Layer::getModel(const State& theState) const
{
  try
  {
    std::string model = (producer ? *producer : theState.getConfig().defaultModel());

    if (theState.isObservation(model))
      return {};

    if (source && *source == "grid")
      return {};

    // chosen valid time is not relevant here if an origintime is set, the data may or may not
    // contain the time
    if (origintime)
      return theState.getModel(model, *origintime);

    // otherwise we try to select the minimal amount of querydata files based on the time or the
    // time interval
    if (!hasValidTime())
      return theState.getModel(model);

    return theState.getModel(model, getValidTimePeriod());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to get required model data!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the projection resolution is in the allowed range
 */
// ----------------------------------------------------------------------

bool Layer::validResolution() const
{
  try
  {
    // Valid if there are no requirements
    if (!minresolution && !maxresolution)
      return true;

    if (!projection.resolution)
      throw Fmi::Exception(
          BCP,
          "Projection resolution has not been specified, cannot use min/maxresolution settings");

    // resolution must be >= minresolution
    if (minresolution && *minresolution <= *projection.resolution)
      return false;

    // resolution must be < maxresolution
    if (maxresolution && *maxresolution > *projection.resolution)
      return false;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Layer::setProjection(const Projection& proj)
{
  try
  {
    projection = proj;
    layers.setProjection(proj);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the set format is allowed for the layer
 *
 * The disabled setting is used first.

 */
// ----------------------------------------------------------------------

bool Layer::validType(const std::string& theType) const
{
  try
  {
    if (!enable.empty() && !disable.empty())
      throw Fmi::Exception(BCP,
                           "Setting disable and enable image formats simultaneously is an error");

    if (!disable.empty())
      return disable.find(theType) == disable.end();

    if (!enable.empty())
      return enable.find(theType) != enable.end();

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a clipping rectangle for the current layer.
 */
// ----------------------------------------------------------------------

void Layer::addClipRect(CTPP::CDT& theCdt,
                        CTPP::CDT& theGlobals,
                        const Fmi::Box& theBox,
                        const State& theState)
{
  // Generate nothing of clipping is not requested
  if (!clip)
    return;

  // Generate nothing if user has overridden the clipping path
  if (!attributes.value("clip-path").empty())
    return;

  // Unique id for the clipPath
  auto id = theState.generateUniqueId();

  // Use if for the current layer
  attributes.add("clip-path", "url(#" + id + ')');

  // Add the SVG clipPath element in front of the actual layer

  CTPP::CDT clip_cdt(CTPP::CDT::HASH_VAL);
  clip_cdt["start"] = "<clipPath";
  clip_cdt["end"] = "</clipPath>";

  Attributes clip_attributes;
  clip_attributes.add("id", id);
  theState.addAttributes(theGlobals, clip_cdt, clip_attributes);

  // The alternative would be to create an inner layer but it would be more code.
  // Note that margin settings are not for this clipRect, they are for clipping
  // of isolines number etc to be slightly larger than this clipRect so that things
  // slightly outside the layer are may still appear in the layer if the rendering
  // is large enough.

  std::string clip_rect = R"(<rect x="0" y="0" width=")" + Fmi::to_string(theBox.width()) +
                          R"(" height=")" + Fmi::to_string(theBox.height()) + R"("/>)";

  clip_cdt["cdata"] = clip_rect;

  theCdt.PushBack(clip_cdt);
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate bounding box for clipping paths
 *
 */
// ----------------------------------------------------------------------

Fmi::Box Layer::getClipBox(const Fmi::Box& theBox) const
{
  // Nothing to modify if the margings are zero
  if (xmargin == 0 && ymargin == 0)
    return theBox;

  auto w = theBox.width();
  auto h = theBox.height();

  // We need to expand the box by the amount specified for the margins

  auto xmin = theBox.xmin();
  auto xmax = theBox.xmax();
  auto ymin = theBox.ymin();
  auto ymax = theBox.ymax();

  // Expanding the clipping rect is a linear operation
  auto dx = (xmax - xmin) * xmargin / w;
  auto dy = (ymax - ymin) * ymargin / h;

  // No need to alter width and height, they are not used for geometry clipping,
  // only for projecting to pixel coordinates.

  return Fmi::Box{xmin - dx, ymin - dy, xmax + dx, ymax + dy, w, h};
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate bounding box for fetching latlon based data
 *
 * 1. expand current box by the margins
 * 2. sample the box boundaries
 * 3. project the sampled coordinates to WGS84
 * 4. establish the WGS84 bounding box from the projected coordinates
 *
 */
// ----------------------------------------------------------------------

std::map<std::string, double> Layer::getClipBoundingBox(const Fmi::Box& theBox,
                                                        const Fmi::SpatialReference& theCRS) const
{
  // Expand world coordinates by the margin settings
  const auto clipbox = getClipBox(theBox);

  // Seek the cache first
  auto hash = clipbox.hashValue();
  Fmi::hash_combine(hash, theCRS.hashValue());

  const auto& obj = g_bbox_cache.find(hash);
  if (obj)
    return *obj;

  // Observations are in WGS84 coordinates

  Fmi::CoordinateTransformation transformation(theCRS, "WGS84");

  // Establish the number of samples along direction. We wish to sample at roughly 5 pixel
  // intervals.

  const std::size_t npixels = 5;
  const std::size_t minsamples = 2;
  const std::size_t w = theBox.width() + 2U * xmargin;
  const std::size_t h = theBox.height() + 2U * ymargin;

  // If the CRS is geographic we need only the corners

  std::size_t wsamples = minsamples;
  std::size_t hsamples = minsamples;

  // Otherwise we need to sample the full image since the poles may be inside the image
  if (!theCRS.isGeographic())
  {
    wsamples = std::max(wsamples, w / npixels);
    hsamples = std::max(hsamples, h / npixels);
  }

  // Sample the image in world coordinates
  std::vector<double> x;
  std::vector<double> y;

  x.reserve(wsamples * hsamples);
  y.reserve(wsamples * hsamples);

  const double dx = (clipbox.xmax() - clipbox.xmin()) / (wsamples - 1);
  const double dy = (clipbox.ymax() - clipbox.ymin()) / (hsamples - 1);
  const double x0 = clipbox.xmin();
  const double y0 = clipbox.ymin();

  for (auto i = 0UL; i < wsamples; i++)
  {
    auto px = x0 + i * dx;
    for (auto j = 0UL; j < hsamples; j++)
    {
      auto py = y0 + j * dy;
      x.push_back(px);
      y.push_back(py);
    }
  }

  // Project all coordinates at once

  if (!theCRS.isGeographic())
    if (!transformation.transform(x, y))
      return {};

  double minlat = 0;
  double minlon = 0;
  double maxlat = 0;
  double maxlon = 0;

  bool lon_missing = true;
  for (auto lon : x)
  {
    if (!std::isnan(lon))
    {
      minlon = (lon_missing ? lon : std::min(minlon, lon));
      maxlon = (lon_missing ? lon : std::max(maxlon, lon));
      lon_missing = false;
    }
  }

  bool lat_missing = true;
  for (auto lat : y)
  {
    if (!std::isnan(lat))
    {
      minlat = (lat_missing ? lat : std::min(minlon, lat));
      maxlat = (lat_missing ? lat : std::max(maxlon, lat));
      lat_missing = false;
    }
  }

  if (lon_missing || lat_missing)
    return {};

  // We may miss the poles since the sample step is 5 pixels. Include the poles explicitly when it
  // seems to be close enough.
  if (minlat < -89)
    minlat = -90;
  if (maxlat > 89)
    maxlat = 90;

  // Build the result in the form wanted by obsengine
  std::map<std::string, double> ret;
  ret["minx"] = minlon;
  ret["maxx"] = maxlon;
  ret["miny"] = minlat;
  ret["maxy"] = maxlat;

  g_bbox_cache.insert(hash, ret);

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the parameter for the layer
 *
 * This method is not used by layers which require no specific
 * parameter. The layers whic do require one call this and get an
 * exception if no parameter has been specified.
 */
// ----------------------------------------------------------------------

#if 0
Parameter Layer::getParameter() const
{
  if(!parameter)
	throw Fmi::Exception(BCP,"Parameter has not been set for all layers");
  return Spine::ParameterFactory::instance().parse(*parameter);
}

#endif

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void Layer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  // By default a layer uses no parameters
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate warnings if needed
 */
// ----------------------------------------------------------------------

void Layer::check_warnings(Warnings& warnings) const
{
  try
  {
    if (!qid.empty())
      ++warnings.qid_counts[qid];
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the hash for the layer
 */
// ----------------------------------------------------------------------

std::size_t Layer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Properties::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(qid));
    Fmi::hash_combine(hash, Fmi::hash_value(minresolution));
    Fmi::hash_combine(hash, Fmi::hash_value(maxresolution));
    Fmi::hash_combine(hash, Fmi::hash_value(enable));
    Fmi::hash_combine(hash, Fmi::hash_value(disable));
    Fmi::hash_combine(hash, Dali::hash_css(css, theState));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(layers, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(visible));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Layer::isFlashOrMobileProducer(const std::string& producer)
{
  return (producer == "flash" || producer == "roadcloud" || producer == "netatmo");
}



bool Layer::isAnimationStepVisible(const State& theState) const
{
  try
  {
    if (theState.animation_enabled)
    {
      for (auto it = animation_effects.effects.begin(); it != animation_effects.effects.end(); ++it)
      {
        if (it->effect == "hide"  &&  theState.animation_loopstep >= it->start_step  && theState.animation_loopstep <= it->end_step)
          return false;
      }
    }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
