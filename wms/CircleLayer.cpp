#include "CircleLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <gis/CoordinateTransformation.h>
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

namespace
{

inline double degtorad(double deg)
{
  return deg * M_PI / 180.0;
}

inline double radtodeg(double rad)
{
  return rad * 180.0 / M_PI;
}

// WGS84 coordinate at desired distance using Lambert's formulas obtained by using ChatGPT

std::pair<double, double> coordinate_at_distance(double lon,
                                                 double lat,
                                                 double bearing,
                                                 double meters)
{
  const double pi = 3.14159265358979323846;  // magic constant

  // WGS84 ellipsoid constants
  const double a = 6378137.0;          // Semi-major axis (meters)
  const double f = 1 / 298.257223563;  // Flattening
  // const double b = (1 - f) * a;        // Semi-minor axis
  // const double e2 = f * (2 - f);       // Square of eccentricity

  // Convert degrees to radians
  double lat1 = degtorad(lat);
  double lon1 = degtorad(lon);
  double alpha1 = degtorad(bearing);

  // Compute reduced latitude
  double tanU1 = (1 - f) * tan(lat1);
  double cosU1 = 1.0 / sqrt(1 + tanU1 * tanU1);
  double sinU1 = tanU1 * cosU1;

  // Compute sigma (angular distance on the sphere)
  double sigma = meters / (a * (1 - f));

  // Compute destination coordinates on the auxiliary sphere
  double sinSigma = sin(sigma);
  double cosSigma = cos(sigma);

  double sinLat2 = sinU1 * cosSigma + cosU1 * sinSigma * cos(alpha1);
  double lat2 = atan2(sinLat2, sqrt(1 - sinLat2 * sinLat2));

  double y = sin(alpha1) * sinSigma * cosU1;
  double x = cosSigma - sinU1 * sinLat2;
  double lambda = atan2(y, x);

  // Correct for longitude
  double lon2 = lon1 + lambda;

  // Convert back to geographic latitude
  double phi2 = atan2(tan(lat2), 1 - f);

  // Normalize longitude to -π ... +π
  lon2 = fmod(lon2 + 3 * pi, 2 * pi) - pi;

  // Return degrees
  return std::make_pair(radtodeg(lon2), radtodeg(phi2));
}

// Create a circle with a specific radius in km using a reasonably accurate formula for distances

std::unique_ptr<OGRPolygon> create_circle(double lon, double lat, double kilometers)
{
  auto circle = std::make_unique<OGRPolygon>();
  auto* ring = new OGRLinearRing;

  for (int theta = 0; theta < 360; ++theta)
  {
    auto lonlat = coordinate_at_distance(lon, lat, theta, kilometers * 1000.0);
    ring->addPoint(lonlat.first, lonlat.second);
  }
  ring->closeRings();

  circle->addRingDirectly(ring);
  return circle;
}

}  // namespace
// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void CircleLayer::init(Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Circle-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(keyword, theJson, "");

    auto json = JsonTools::remove(theJson, "places");
    if (!json.isNull())
      JsonTools::extract_set("places", places, json);

    json = JsonTools::remove(theJson, "circles");
    if (json.isNull())
      throw Fmi::Exception(BCP, "Circle-layer must define at least one circle");

    if (json.isObject())
    {
      Circle circle;  // one circle only
      circle.init(json, theConfig);
      circles.emplace_back(circle);
    }
    else if (json.isArray())
    {
      for (auto& j : json)  // several circles
      {
        Circle circle;
        circle.init(j, theConfig);
        circles.emplace_back(circle);
      }
    }
    else
      throw Fmi::Exception(
          BCP, "Circle-layer circles setting must define a circle or an array of circles");
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

void CircleLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (circles.empty())
      throw Fmi::Exception(BCP, "CircleLayer must define at least one circle");

    if (!validLayer(theState))
      return;

    // latlon coordinates to be used

    struct LonLat
    {
      double lon;
      double lat;
    };

    Spine::LocationList locations;

    // Resolve keyword & places

    const auto& geoengine = theState.getGeoEngine();

    Locus::QueryOptions options;
    if (!keyword.empty())
      auto locations = geoengine.keywordSearch(options, keyword);

    if (!places.empty())
    {
      for (const auto& place : places)
      {
        auto newlocations = geoengine.nameSearch(options, place);
        locations.splice(locations.end(), newlocations);
      }
    }

    // Get projection details

    auto q = getModel(theState);

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Needed coordinate transformation
    Fmi::SpatialReference wgs84("WGS84");
    Fmi::CoordinateTransformation transformation(wgs84, *crs);

    // Generate a <g> group of circles

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Common attributes
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Process one location at a time

    const auto precision = theState.getPrecision("circle");

    for (const auto& location : locations)
    {
      auto lon = location->longitude;
      auto lat = location->latitude;
      for (const auto& circle : circles)
      {
        auto polygon = create_circle(lon, lat, circle.radius);
        if (!polygon || polygon->IsEmpty() != 0)
          continue;

        OGRGeometryPtr geom(transformation.transformGeometry(*polygon));
        if (!geom || geom->IsEmpty() != 0)
          continue;

        geom.reset(Fmi::OGR::polyclip(*geom, clipbox));
        if (!geom || geom->IsEmpty() != 0)
          continue;

        // Generate unique ID for the circle
        std::string iri = qid + (qid.empty() ? "" : ".") + theState.makeQid("circle");

        if (!theState.addId(iri))
          throw Fmi::Exception(BCP, "Non-unique ID assigned to circle").addParameter("ID", iri);

        CTPP::CDT circle_cdt(CTPP::CDT::HASH_VAL);
        circle_cdt["iri"] = iri;

        std::string arcNumbers;
        std::string arcCoordinates;

        auto coords = Geometry::toString(*geom,
                                         theState.getType(),
                                         box,
                                         crs,
                                         precision,
                                         theState.arcHashMap,
                                         theState.arcCounter,
                                         arcNumbers,
                                         arcCoordinates);
        if (!coords.empty())
          circle_cdt["data"] = coords;

        circle_cdt["type"] = Geometry::name(*geom, theState.getType());
        circle_cdt["layertype"] = "circle";

        circle_cdt["longitude"] = lon;
        circle_cdt["latitude"] = lat;
        circle_cdt["radius"] = circle.radius;

        theState.addPresentationAttributes(circle_cdt, css, attributes, circle.attributes);

        if (theState.getType() == "topojson")
        {
          if (!arcNumbers.empty())
            circle_cdt["arcs"] = arcNumbers;

          if (!arcCoordinates.empty())
          {
            CTPP::CDT arc_cdt(CTPP::CDT::HASH_VAL);
            arc_cdt["data"] = arcCoordinates;
            theGlobals["arcs"][theState.insertCounter] = arc_cdt;
            theState.insertCounter++;
          }
          group_cdt["paths"][iri] = circle_cdt;
        }
        else
        {
          theGlobals["paths"][iri] = circle_cdt;
        }

        // Add the SVG use element
        CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
        tag_cdt["start"] = "<use";
        tag_cdt["end"] = "/>";
        theState.addAttributes(theGlobals, tag_cdt, circle.attributes);
        tag_cdt["attributes"]["xlink:href"] = "#" + iri;
        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    std::string objectKey = "circle:" + qid;
    theGlobals["objects"][objectKey] = group_cdt;

    theGlobals["bbox"] = Fmi::to_string(box.xmin()) + "," + Fmi::to_string(box.ymin()) + "," +
                         Fmi::to_string(box.xmax()) + "," + Fmi::to_string(box.ymax());
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -static_cast<int>(precision));

    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t CircleLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(keyword));
    Fmi::hash_combine(hash, Fmi::hash_value(places));
    for (const auto& circle : circles)
      Fmi::hash_combine(hash, circle.hash_value(theState));
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