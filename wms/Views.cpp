#include "Views.h"
#include "Hash.h"

#include <ctpp2/CDT.hpp>
#include <spine/Exception.h>

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

void Views::init(const Json::Value& theJson,
                 const State& theState,
                 const Config& theConfig,
                 const Properties& theProperties)
{
  try
  {
    if (!theJson.isArray())
      throw Spine::Exception(BCP, "Views setting must be an array");

    for (unsigned int i = 0; i < theJson.size(); i++)
    {
      const Json::Value& json = theJson[i];
      boost::shared_ptr<View> view(new View);
      view->init(json, theState, theConfig, theProperties);
      views.push_back(view);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
