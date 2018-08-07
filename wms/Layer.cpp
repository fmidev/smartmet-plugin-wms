// ======================================================================

#include "Layer.h"
#include "Config.h"
#include "Defs.h"
#include "Hash.h"
#include "Layer.h"
#include "LayerFactory.h"
#include "State.h"
#include "View.h"
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <boost/move/make_unique.hpp>
#include <ctpp2/CDT.hpp>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <stdexcept>

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

void Layer::init(const Json::Value& theJson,
                 const State& theState,
                 const Config& theConfig,
                 const Properties& theProperties)
{
  try
  {
    Properties::init(theJson, theState, theConfig, theProperties);

    Json::Value nulljson;

    auto json = theJson.get("qid", nulljson);
    if (!json.isNull())
      qid = json.asString();

    json = theJson.get("minresolution", nulljson);
    if (!json.isNull())
      minresolution = json.asDouble();

    json = theJson.get("maxresolution", nulljson);
    if (!json.isNull())
      maxresolution = json.asDouble();

    json = theJson.get("enable", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_set("enable", enable, json);

    json = theJson.get("disable", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_set("disable", disable, json);

    json = theJson.get("css", nulljson);
    if (!json.isNull())
      css = json.asString();

    json = theJson.get("attributes", nulljson);
    if (!json.isNull())
      attributes.init(json, theConfig);

    json = theJson.get("layers", nulljson);
    if (!json.isNull())
      layers.init(json, theState, theConfig, *this);

    json = theJson.get("layer_type", nulljson);
    if (!json.isNull())
      type = json.asString();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the producer refers to observations
 */
// ----------------------------------------------------------------------

bool Layer::isObservation(const State& theState) const
{
  std::string model = (producer ? *producer : theState.getConfig().defaultModel());

#ifdef WITHOUT_OBSERVATION
  return false;
#else
  if (theState.getConfig().obsEngineDisabled())
    return false;

  auto observers = theState.getObsEngine().getValidStationTypes();
  return (observers.find(model) != observers.end());
#endif
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
    if (isObservation(theState))
      return {};

    std::string model = (producer ? *producer : theState.getConfig().defaultModel());

    if (!origintime)
      return theState.get(model);
    return theState.get(model, *origintime);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to get required model data!");
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
      throw Spine::Exception(
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      throw Spine::Exception(BCP,
                             "Setting disable and enable image formats simultaneously is an error");

    if (!disable.empty())
      return disable.find(theType) == disable.end();

    if (!enable.empty())
      return enable.find(theType) != enable.end();

    return true;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
                        State& theState)
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
 * Note: Projecting individual coordinates may fail. We project
 * one coordinate at a time to make sure we get the bounding box for
 * whatever is valid. In fact, simply expanding the box by the margins
 * may have resulted in world coordinates which are not valid. However,
 * by sampling the boundary sufficiently well such problems can be ignored.
 *
 */
// ----------------------------------------------------------------------

std::map<std::string, double> Layer::getClipBoundingBox(
    const Fmi::Box& theBox, const boost::shared_ptr<OGRSpatialReference>& theCRS) const
{
  // Expand world coordinates by the margin settings
  const auto clipbox = getClipBox(theBox);

  // Observations are in WGS84 coordinates

  auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
  OGRErr err = wgs84->SetFromUserInput("WGS84");
  if (err != OGRERR_NONE)
    throw Spine::Exception(BCP, "GDAL does not understand WGS84");

  // Create the transformation from image world coordinates to WGS84 coordinates
  boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
      OGRCreateCoordinateTransformation(theCRS.get(), wgs84.get()));
  if (transformation == nullptr)
    throw Spine::Exception(
        BCP, "Failed to create the needed coordinate transformation when drawing wind arrows");

  // Establish the number of samples along each edge. We wish to sample at roughly 5 pixel
  // intervals.

  const int npixels = 5;
  const int minsamples = 2;
  const int w = theBox.width() + 2 * xmargin;
  const int h = theBox.height() + 2 * ymargin;

  // If the CRS is geographic we need only the corners

  int wsamples = minsamples;
  int hsamples = minsamples;

  // Otherwise we need to sample the edges
  if (theCRS->IsGeographic() == 0)
  {
    wsamples = std::max(wsamples, w / npixels);
    hsamples = std::max(hsamples, h / npixels);
  }

  // Sample the edges in world coordinates
  std::vector<double> x;
  std::vector<double> y;

  // Bottom edge
  for (int i = 0; i < wsamples; i++)
  {
    x.push_back(clipbox.xmin() + (clipbox.xmax() - clipbox.xmin()) * i / (wsamples - 1));
    y.push_back(clipbox.ymin());
  }

  // Top edge
  for (int i = 0; i < wsamples; i++)
  {
    x.push_back(clipbox.xmin() + (clipbox.xmax() - clipbox.xmin()) * i / (wsamples - 1));
    y.push_back(clipbox.ymax());
  }

  // Left edge
  for (int i = 0; i < hsamples; i++)
  {
    x.push_back(clipbox.xmin());
    y.push_back(clipbox.ymin() + (clipbox.ymax() - clipbox.ymin()) * i / (hsamples - 1));
  }

  // Right edge
  for (int i = 0; i < hsamples; i++)
  {
    x.push_back(clipbox.xmax());
    y.push_back(clipbox.ymin() + (clipbox.ymax() - clipbox.ymin()) * i / (hsamples - 1));
  }

  // Project one at a time and extract the latlon bounding box
  double minlat = 0;
  double minlon = 0;
  double maxlat = 0;
  double maxlon = 0;

  bool uninitialized = true;

  for (std::size_t i = 0; i < x.size(); i++)
  {
    if (theCRS->IsGeographic() == 0)
      if (transformation->Transform(1, &x[i], &y[i]) == 0)
        continue;

    minlon = (uninitialized ? x[i] : std::min(minlon, x[i]));
    maxlon = (uninitialized ? x[i] : std::max(minlon, x[i]));
    minlat = (uninitialized ? y[i] : std::min(minlat, y[i]));
    maxlat = (uninitialized ? y[i] : std::max(minlat, y[i]));

    uninitialized = false;
  }

  // Return empty bounding box if unable to establish bounding box. In some cases
  // the projection tiling is valid, and we do not wish to error. Example: projections
  // which show the earth as a sphere, but the tiles are empty at the corners.

  if (uninitialized)
    return {};

  // Build the result in the form wanted by obsengine
  std::map<std::string, double> ret;
  ret["minx"] = minlon;
  ret["maxx"] = maxlon;
  ret["miny"] = minlat;
  ret["maxy"] = maxlat;
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
	throw Spine::Exception(BCP,"Parameter has not been set for all layers");
  return Spine::ParameterFactory::instance().parse(*parameter);
}

#endif

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
    Dali::hash_combine(hash, Dali::hash_value(qid));
    Dali::hash_combine(hash, Dali::hash_value(minresolution));
    Dali::hash_combine(hash, Dali::hash_value(maxresolution));
    Dali::hash_combine(hash, Dali::hash_value(enable));
    Dali::hash_combine(hash, Dali::hash_value(disable));
    Dali::hash_combine(hash, Dali::hash_css(css, theState));
    Dali::hash_combine(hash, Dali::hash_value(attributes, theState));
    Dali::hash_combine(hash, Dali::hash_value(layers, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
