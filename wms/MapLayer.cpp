//======================================================================

#include "MapLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Layer.h"
#include "State.h"
#include <boost/foreach.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <spine/Exception.h>

// TODO:
#include <boost/timer/timer.hpp>

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

void MapLayer::init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Map JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract all members

    Json::Value nulljson;

    auto json = theJson.get("map", nulljson);
    if (!json.isNull())
      map.init(json, theConfig);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void MapLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::string report = "MapLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // Get projection details

    bool has_data_proj = (projection.crs && *projection.crs == "data");

    // Update projection from querydata if necessary
    if (has_data_proj)
    {
      // Establish the data
      auto q = getModel(theState);
      projection.update(q);
    }

    auto crs = projection.getCRS();
    const Fmi::Box& box = projection.getBox();

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Fetch the shape in our projection

    OGRGeometryPtr geom;

    const auto& gis = theState.getGisEngine();
    {
      std::string report = "getShape finished in %t sec CPU, %w sec real\n";
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer.reset(new boost::timer::auto_cpu_timer(2, report));
      geom = gis.getShape(crs.get(), map.options);

      if (!geom)
      {
        std::string msg =
            "Requested map data is empty: '" + map.options.schema + '.' + map.options.table + "'";
        if (map.options.minarea)
          msg += " Is the minarea limit too large?";

        throw Spine::Exception(BCP, msg);
      }
    }

    // Clip it

    {
      std::string report = "polyclip finished in %t sec CPU, %w sec real\n";
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer.reset(new boost::timer::auto_cpu_timer(2, report));
      if (map.lines)
        geom.reset(Fmi::OGR::lineclip(*geom, clipbox));  // fast and hence not cached in gisengine
      else
        geom.reset(Fmi::OGR::polyclip(*geom, clipbox));  // fast and hence not cached in gisengine
    }

    // We might zoom in so close that some geometry becomes invisible - just don't generate anything
    if (!geom)
      return;

    // Store the path. Note that we allow duplicate IDs since multiple views may
    // refer to the same map.

    std::string iri = qid;

    {
      std::string report = "Generating coordinate data finished in %t sec CPU, %w sec real\n";
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer.reset(new boost::timer::auto_cpu_timer(2, report));

      CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
      map_cdt["iri"] = iri;
      map_cdt["type"] = Geometry::name(*geom, theState.getType());
      map_cdt["layertype"] = "map";
      map_cdt["data"] = Geometry::toString(*geom, theState.getType(), box, crs);

      theState.addPresentationAttributes(map_cdt, css, attributes);

      theGlobals["paths"][iri] = map_cdt;
    }

    // Do not produce a use-statement for empty data or in the header
    if (!geom->IsEmpty())
    {
      // Update the globals

      if (css)
      {
        std::string name = theState.getCustomer() + "/" + *css;
        theGlobals["css"][name] = theState.getStyle(*css);
      }

      // Clip if necessary

      addClipRect(theLayersCdt, theGlobals, box, theState);

      // The output consists of a use tag only. We could output a group
      // only without any tags, but the output is nicer looking this way

      CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
      group_cdt["start"] = "";
      group_cdt["end"] = "";
      group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);

      // Add the SVG use element
      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";
      // Add attributes to the tag
      theState.addAttributes(theGlobals, tag_cdt, attributes);
      tag_cdt["attributes"]["xlink:href"] = "#" + iri;
      // Tag to group
      group_cdt["tags"].PushBack(tag_cdt);
      // Group to layers
      theLayersCdt.PushBack(group_cdt);
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t MapLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, Dali::hash_value(map, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
