#include "WKTLayer.h"
#include "Geometry.h"
#include "Hash.h"
#include "Layer.h"
#include "State.h"
#include <spine/Exception.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>

#include <gis/OGR.h>
#include <gis/Types.h>

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

void WKTLayer::init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "WKT-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("wkt", nulljson);
    if (!json.isNull())
      wkt = json.asString();

    json = theJson.get("resolution", nulljson);
    if (!json.isNull())
      resolution = json.asDouble();

    json = theJson.get("relativeresolution", nulljson);
    if (!json.isNull())
      relativeresolution = json.asDouble();
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

void WKTLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (wkt.empty())
      throw Spine::Exception(BCP, "WKTLayer WKT must be defined and be non-empty");

    if (!validLayer(theState))
      return;

    if (resolution && relativeresolution)
      throw Spine::Exception(BCP, "Cannot set both resolution and relativeresolution for WKT");

    // Get projection details

    auto q = getModel(theState);

    projection.update(q);
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Create the shape

    OGRSpatialReference* wgs84(new OGRSpatialReference);
    wgs84->SetFromUserInput("WGS84");

    char* cwkt = const_cast<char*>(wkt.c_str());
    OGRGeometry* ogeom;
    OGRErr err = OGRGeometryFactory::createFromWkt(&cwkt, wgs84, &ogeom);

    if (err != OGRERR_NONE)
    {
      delete wgs84;
      throw SmartMet::Spine::Exception(BCP, "Failed to convert WKT to OGRGeometry");
    }

    OGRGeometryPtr geom(ogeom);

    // Resample to get more accuracy if so requested

    if (resolution)
    {
    }

    if (relativeresolution)
    {
    }

    // Establish coordinate transformation from WGS84 to image

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(crs.get(), wgs84));

    if (!transformation)
      throw Spine::Exception(BCP, "Failed to create the needed coordinate transformation for WKT!");

    err = geom->transform(transformation.get());
    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP, "Failed to project the WKT to image coordinates");

    // Clip the geometry to the bounding box

    OGRGeometryPtr geom2(Fmi::OGR::polyclip(*geom, clipbox));

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    std::string iri = qid;
    if (iri.empty())
      iri = theState.generateUniqueId();

    CTPP::CDT wkt_cdt(CTPP::CDT::HASH_VAL);
    wkt_cdt["iri"] = iri;
    wkt_cdt["data"] = Geometry::toString(*geom2, theState.getType(), box, crs);
    wkt_cdt["type"] = Geometry::name(*geom2, theState.getType());
    wkt_cdt["layertype"] = "wkt";

    theGlobals["paths"][iri] = wkt_cdt;

    if (!theState.inDefs())
    {
      addClipRect(theLayersCdt, theGlobals, box, theState);

      // The output consists of a rect tag only. We could output a group
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

std::size_t WKTLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, Dali::hash_value(wkt));
    boost::hash_combine(hash, Dali::hash_value(resolution));
    boost::hash_combine(hash, Dali::hash_value(relativeresolution));
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