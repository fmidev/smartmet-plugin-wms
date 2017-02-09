// ======================================================================

#include "Layer.h"
#include "Config.h"
#include "Defs.h"
#include "Hash.h"
#include "State.h"
#include "View.h"
#include "LayerFactory.h"
#include "Layer.h"
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
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
 * \brief Get the model data
 */
// ----------------------------------------------------------------------

SmartMet::Engine::Querydata::Q Layer::getModel(const State& theState) const
{
  try
  {
    std::string model = (producer ? *producer : theState.getConfig().defaultModel());
    return theState.get(model);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

#if 0
    // ----------------------------------------------------------------------
    /*!
     * \brief Establish the parameter for the layer
     *
     * This method is not used by layers which require no specific
     * parameter. The layers whic do require one call this and get an
     * exception if no parameter has been specified.
     */
    // ----------------------------------------------------------------------

    Parameter Layer::getParameter() const
    {
      if(!parameter)
		throw SmartMet::Spine::Exception(BCP,"Parameter has not been set for all layers");
      return SmartMet::Spine::ParameterFactory::instance().parse(*parameter);
    }

    // ----------------------------------------------------------------------
    /*!
     * \brief Establish the time for the layer
     *
     * This method is not used by layers which require no specific time.
     * The layers which do require one call this and get an exception if
     * no time has been specified.
     */
    // ----------------------------------------------------------------------

    boost::posix_time::ptime Layer::getValidTime() const
    {
      if(!time)
		throw SmartMet::Spine::Exception(BCP,"Time has not been set for all layers");

      auto valid_time = *time;
      if(time_offset)
		valid_time += boost::posix_time::minutes(*time_offset);

      return valid_time;
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
