#include "Views.h"
#include "Hash.h"

#include <ctpp2/CDT.hpp>
#include <macgyver/Exception.h>

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

void Views::init(Json::Value& theJson,
                 const State& theState,
                 const Config& theConfig,
                 const Properties& theProperties)
{
  try
  {
    if (!theJson.isArray())
      throw Fmi::Exception(BCP, "Views setting must be an array");

    for (auto& json : theJson)
    {
      std::shared_ptr<View> view(new View);
      view->init(json, theState, theConfig, theProperties);
      views.push_back(view);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate warnings if needed
 */
// ----------------------------------------------------------------------

void Views::check_warnings(Warnings& warnings) const
{
  try
  {
    for (const auto& view : views)
      view->check_warnings(warnings);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the content
 */
// ----------------------------------------------------------------------

void Views::generate(CTPP::CDT& theGlobals, State& theState)
{
  try
  {
    for (auto& view : views)
    {
      CTPP::CDT view_hash(CTPP::CDT::HASH_VAL);
      view->generate(theGlobals, view_hash, theState);
      theGlobals["views"].PushBack(view_hash);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void Views::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  for (const auto& view : views)
    view->addGridParameterInfo(infos, theState);
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value = combined hash from all views
 */
// ----------------------------------------------------------------------

std::size_t Views::hash_value(const State& theState) const
{
  try
  {
    return Dali::hash_value(views, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
