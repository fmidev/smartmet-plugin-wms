#include "GridLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Layer.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <engines/querydata/Engine.h>
#include <gis/CoordinateMatrixAnalysis.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <macgyver/Exception.h>
#include <ogr_geometry.h>

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

void GridLayer::init(const Json::Value& theJson,
                     const State& theState,
                     const Config& theConfig,
                     const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Grid-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void GridLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Get projection details

    auto q = getModel(theState);

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Get the grid

    const auto& qEngine = theState.getQEngine();
    auto coords = qEngine.getWorldCoordinates(q, crs);

    // Analyze the grid, do not render invalid cells
    auto analysis = Fmi::analysis(*coords);

    // Create the shape

    auto* g = new OGRGeometryCollection;

    std::size_t j1 = 0;
    std::size_t j2 = analysis.valid.height() - 1;
    std::size_t i1 = 0;
    std::size_t i2 = analysis.valid.width() - 1;

    for (std::size_t j = j1; j < j2; j++)
      for (std::size_t i = i1; i < i2; i++)
      {
        if (analysis.valid(i, j))
        {
          auto* poly = new OGRPolygon;
          auto* ring = new OGRLinearRing;
          ring->addPoint(coords->x(i, j), coords->y(i, j));
          ring->addPoint(coords->x(i + 1, j), coords->y(i + 1, j));
          ring->addPoint(coords->x(i + 1, j + 1), coords->y(i + 1, j + 1));
          ring->addPoint(coords->x(i, j + 1), coords->y(i, j + 1));
          ring->addPoint(coords->x(i, j), coords->y(i, j));
          poly->addRingDirectly(ring);
          g->addGeometryDirectly(poly);
        }
      }

    OGRGeometryPtr geom(g);
    // geom->assignSpatialReference(crs.get()->Clone()); // not needed in following code

    // Clip the geometry to the bounding box

    auto* geom2 = Fmi::OGR::polyclip(*geom, clipbox);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    std::string iri = qid;
    if (iri.empty())
      iri = theState.generateUniqueId();
    else if (!theState.addId(iri))
      throw Fmi::Exception(BCP, "Non-unique ID assigned to Grid layer").addParameter("ID", iri);

    const double precision = 0.1;  // about 1 pixel accuracy should be sufficient
    CTPP::CDT grid_cdt(CTPP::CDT::HASH_VAL);
    grid_cdt["iri"] = iri;
    grid_cdt["data"] = Geometry::toString(*geom2, theState.getType(), box, crs, precision);
    grid_cdt["type"] = Geometry::name(*geom2, theState.getType());
    grid_cdt["layertype"] = "grid";

    theGlobals["paths"][iri] = grid_cdt;

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // The output consists of a path tag only. We could output a group
    // only without any tags, but the output is nicer looking this way

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "";
    group_cdt["end"] = "";
    group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);

    // Add the SVG use element for the path data

    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";
    theState.addAttributes(theGlobals, tag_cdt, attributes);
    tag_cdt["attributes"]["xlink:href"] = "#" + iri;

    group_cdt["tags"].PushBack(tag_cdt);
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Dali

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t GridLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
