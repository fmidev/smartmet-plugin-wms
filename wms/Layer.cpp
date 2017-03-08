// ======================================================================

#include "Layer.h"
#include "Config.h"
#include "Defs.h"
#include "Hash.h"
#include "State.h"
#include "View.h"
#include "LayerFactory.h"
#include "Layer.h"
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <gis/Box.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
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
      SmartMet::Spine::JSON::extract_set("enable", enable, json);

    json = theJson.get("disable", nulljson);
    if (!json.isNull())
      SmartMet::Spine::JSON::extract_set("disable", disable, json);

    json = theJson.get("css", nulljson);
    if (!json.isNull())
      css = json.asString();

    json = theJson.get("attributes", nulljson);
    if (!json.isNull())
      attributes.init(json, theConfig);

    json = theJson.get("layers", nulljson);
    if (!json.isNull())
      layers.init(json, theState, theConfig, *this);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

SmartMet::Engine::Querydata::Q Layer::getModel(const State& theState) const
{
  try
  {
    if (isObservation(theState))
      return {};

    std::string model = (producer ? *producer : theState.getConfig().defaultModel());
    return theState.get(model);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Failed to get required model data!", NULL);
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
      throw SmartMet::Spine::Exception(
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
      throw SmartMet::Spine::Exception(
          BCP, "Setting disable and enable image formats simultaneously is an error");

    if (!disable.empty())
      return disable.find(theType) == disable.end();

    if (!enable.empty())
      return enable.find(theType) != enable.end();

    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
	throw SmartMet::Spine::Exception(BCP,"Parameter has not been set for all layers");
  return SmartMet::Spine::ParameterFactory::instance().parse(*parameter);
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
    boost::hash_combine(hash, Dali::hash_value(qid));
    boost::hash_combine(hash, Dali::hash_value(minresolution));
    boost::hash_combine(hash, Dali::hash_value(maxresolution));
    boost::hash_combine(hash, Dali::hash_value(enable));
    boost::hash_combine(hash, Dali::hash_value(disable));
    boost::hash_combine(hash, Dali::hash_css(css, theState));
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
    boost::hash_combine(hash, Dali::hash_value(layers, theState));
    return hash;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
