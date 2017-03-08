#include "PostGISLayer.h"
#include "State.h"

#include <ctpp2/CDT.hpp>

#include <spine/Exception.h>
#include <engines/gis/MapOptions.h>
#include <gis/OGR.h>

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
      throw SmartMet::Spine::Exception(BCP, "PostGISLayer projection not set");

    auto crs = projection.getCRS();

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
    if (!theState.inDefs())
    {
      group_cdt["start"] = "<g";
      group_cdt["end"] = "</g>";
      // Add attributes to the group, not the areas
      theState.addAttributes(theGlobals, group_cdt, attributes);
    }

    SmartMet::Engine::Gis::MapOptions mapOptions;
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

      OGRGeometryPtr geom = getShape(theState, crs.get(), mapOptions);

      if (geom && !geom->IsEmpty())
      {
        OGRGeometryPtr geom2(Fmi::OGR::polyclip(*geom, clipbox));

        if (geom2 && !geom2->IsEmpty())
        {
          // Store the path with unique ID
          std::string iri = (qid + std::to_string(mapid++));
          theGlobals["paths"][iri] = Fmi::OGR::exportToSvg(*geom2, box, 1);

          if (!theState.inDefs())
          {
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
    }
    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
