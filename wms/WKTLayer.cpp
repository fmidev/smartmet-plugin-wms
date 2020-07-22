#include "WKTLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Layer.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <gis/OGR.h>
#include <gis/Types.h>
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

    precision = theConfig.defaultPrecision("wkt");

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

    json = theJson.get("precision", nulljson);
    if (!json.isNull())
      precision = json.asDouble();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    auto wgs84 = theState.getGisEngine().getSpatialReference("WGS84");

    char* cwkt = const_cast<char*>(wkt.c_str());  // NOLINT(cppcoreguidelines-pro-type-const-cast)
    OGRGeometry* ogeom = nullptr;
    OGRErr err = OGRGeometryFactory::createFromWkt(&cwkt, wgs84.get(), &ogeom);

    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP, "Failed to convert WKT to OGRGeometry");

    OGRGeometryPtr geom(ogeom);

    // Resample to get more accuracy if so requested

    if (resolution || relativeresolution)
    {
      if (!projection.resolution)
        throw Spine::Exception(BCP,
                               "Cannot segmentize WKT layer if projection resolution is not known");

      double res = 0;
      if (resolution)
        res = *resolution;
      else
        res = (*projection.resolution) * (*relativeresolution);

      // Convert resolution in km to resolution in degrees (approximation only)

      double pi = boost::math::constants::pi<double>();
      double circumference = 2 * pi * 6371.220;  // km
      double delta = 360 * res / circumference;  // part of the earth circumference in

      geom->segmentize(delta);
    }

    // Establish coordinate transformation from WGS84 to image

    err = geom->transformTo(crs.get());
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
    else if (!theState.addId(iri))
      throw Spine::Exception(BCP, "Non-unique ID assigned to WKT layer").addParameter("ID", iri);

    CTPP::CDT wkt_cdt(CTPP::CDT::HASH_VAL);
    wkt_cdt["iri"] = iri;
    wkt_cdt["data"] = Geometry::toString(*geom2, theState, box, crs, precision);
    wkt_cdt["type"] = Geometry::name(*geom2, theState);
    wkt_cdt["layertype"] = "wkt";

    theGlobals["paths"][iri] = wkt_cdt;

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
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    Dali::hash_combine(hash, Dali::hash_value(wkt));
    Dali::hash_combine(hash, Dali::hash_value(resolution));
    Dali::hash_combine(hash, Dali::hash_value(relativeresolution));
    Dali::hash_combine(hash, Dali::hash_value(precision));
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
