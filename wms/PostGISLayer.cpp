#include "PostGISLayer.h"
#include "Geometry.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <engines/gis/MapOptions.h>
#include <gis/OGR.h>
#include <spine/Exception.h>
#include <ogr_geometry.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
void PostGISLayer::init(const Json::Value& theJson,
                        const State& theState,
                        const Config& theConfig,
                        const Properties& theProperties)
{
  try
  {
    PostGISLayerBase::init(theJson, theState, theConfig, theProperties);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void PostGISLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Get projection details
    if (!projection.crs)
      throw Spine::Exception(BCP, "PostGISLayer projection not set");

    const auto& crs = projection.getCRS();

    // Update the globals
    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Clip if necessary

    const auto& box = projection.getBox();
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Generate areas as use tag statements inside <g>..</g>
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    // Add attributes to the group, not the areas
    theState.addAttributes(theGlobals, group_cdt, attributes);

    Engine::Gis::MapOptions mapOptions;
    mapOptions.pgname = pgname;
    mapOptions.schema = schema;
    mapOptions.table = table;

    unsigned int mapid(1);  // id to concatenate to iri to make it unique
                            // Get the polygons and store them into the template engine
    for (const PostGISLayerFilter& filter : filters)
    {
      if (time_condition && filter.where)
        mapOptions.where = (*filter.where + " AND " + *time_condition);
      else if (time_condition)
        mapOptions.where = time_condition;
      else if (filter.where)
        mapOptions.where = filter.where;

      OGRGeometryPtr geom = getShape(theState, crs, mapOptions);

      if (geom && geom->IsEmpty() == 0)
      {
        if (isLines())
          geom.reset(Fmi::OGR::lineclip(*geom, clipbox));
        else
          geom.reset(Fmi::OGR::polyclip(*geom, clipbox));

        if (geom && geom->IsEmpty() == 0)
        {
          // Store the path
          std::string iri = (qid + std::to_string(mapid++));

          CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
          map_cdt["iri"] = iri;
          map_cdt["type"] = Geometry::name(*geom, theState.getType());
          map_cdt["layertype"] = "postgis";
          map_cdt["data"] = Geometry::toString(*geom, theState.getType(), box, crs, precision);
          theState.addPresentationAttributes(map_cdt, css, attributes);
          theGlobals["paths"][iri] = map_cdt;

          // Add the SVG use element
          CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
          tag_cdt["start"] = "<use";
          tag_cdt["end"] = "/>";
          theState.addAttributes(theGlobals, tag_cdt, filter.attributes);
          tag_cdt["attributes"]["xlink:href"] = "#" + iri;
          group_cdt["tags"].PushBack(tag_cdt);
        }
      }
    }
    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t PostGISLayer::hash_value(const State& theState) const
{
  try
  {
    return PostGISLayerBase::hash_value(theState);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
