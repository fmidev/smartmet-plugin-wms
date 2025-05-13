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

OGRGeometryPtr create_circle(double lon, double lat, double kilometers)
{
  auto circle = std::make_shared<OGRPolygon>();
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

void find_top_point(double& x, double& y, const OGRGeometryPtr& geom)
{
  if (!geom || geom->IsEmpty() == 1)
    return;
  const auto* poly = dynamic_cast<const OGRPolygon*>(&*geom);
  if (poly == nullptr)
    return;
  const auto* ring = poly->getExteriorRing();
  if (ring->IsEmpty() == 1)
    return;
  x = ring->getX(0);
  y = ring->getY(0);
  for (int i = 1; i < ring->getNumPoints(); i++)
  {
    if (ring->getY(i) > y)
    {
      x = ring->getX(i);
      y = ring->getY(i);
    }
  }
}

void find_bottom_point(double& x, double& y, const OGRGeometryPtr& geom)
{
  if (!geom || geom->IsEmpty() == 1)
    return;
  const auto* poly = dynamic_cast<const OGRPolygon*>(&*geom);
  if (poly == nullptr)
    return;
  const auto* ring = poly->getExteriorRing();
  if (ring->IsEmpty() == 1)
    return;
  x = ring->getX(0);
  y = ring->getY(0);
  for (int i = 1; i < ring->getNumPoints(); i++)
  {
    if (ring->getY(i) < y)
    {
      x = ring->getX(i);
      y = ring->getY(i);
    }
  }
}

void find_left_point(double& x, double& y, const OGRGeometryPtr& geom)
{
  if (!geom || geom->IsEmpty() == 1)
    return;
  const auto* poly = dynamic_cast<const OGRPolygon*>(&*geom);
  if (poly == nullptr)
    return;
  const auto* ring = poly->getExteriorRing();
  if (ring->IsEmpty() == 1)
    return;
  x = ring->getX(0);
  y = ring->getY(0);
  for (int i = 1; i < ring->getNumPoints(); i++)
  {
    if (ring->getX(i) < x)
    {
      x = ring->getX(i);
      y = ring->getY(i);
    }
  }
}

void find_right_point(double& x, double& y, const OGRGeometryPtr& geom)
{
  if (!geom || geom->IsEmpty() == 1)
    return;
  const auto* poly = dynamic_cast<const OGRPolygon*>(&*geom);
  if (poly == nullptr)
    return;
  const auto* ring = poly->getExteriorRing();
  if (ring->IsEmpty() == 1)
    return;
  x = ring->getX(0);
  y = ring->getY(0);
  for (int i = 1; i < ring->getNumPoints(); i++)
  {
    if (ring->getX(i) > x)
    {
      x = ring->getX(i);
      y = ring->getY(i);
    }
  }
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

    JsonTools::remove_string(keyword, theJson, "keyword");
    JsonTools::remove_string(features, theJson, "features");
    JsonTools::remove_bool(lines, theJson, "lines");

    auto json = JsonTools::remove(theJson, "places");
    if (!json.isNull())
      JsonTools::extract_set("places", places, json);

    json = JsonTools::remove(theJson, "geoids");
    if (!json.isNull())
      JsonTools::extract_set("geoids", geoids, json);

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

    json = JsonTools::remove(theJson, "labels");
    labels.init(json, theConfig);
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

    // Resolve keyword, places, geoids

    const auto& geoengine = theState.getGeoEngine();

    Locus::QueryOptions options;

    if (!keyword.empty())
      locations = geoengine.keywordSearch(options, keyword);

    if (!geoids.empty())
    {
      for (const auto& geoid : geoids)
      {
        auto newlocations = geoengine.idSearch(options, geoid);

        if (newlocations.size() > 1)  // use only the first match
          newlocations.resize(1);

        locations.splice(locations.end(), newlocations);
      }
    }

    if (!places.empty())
    {
      if (!features.empty())
        options.SetFeatures(features);  // done only for places
      for (const auto& place : places)
      {
        auto newlocations = geoengine.nameSearch(options, place);

        if (newlocations.size() > 1)  // use only the first match
          newlocations.resize(1);

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

    // First generate the circles in WorldXY coordinates without clipping so that we can
    // pick min/max x/y coordinates for label placements

    std::vector<OGRGeometryPtr> all_circles;

    for (const auto& location : locations)
    {
      auto lon = location->longitude;
      auto lat = location->latitude;
      for (const auto& circle : circles)
      {
        auto polygon = create_circle(lon, lat, circle.radius);
        if (polygon && polygon->IsEmpty() == 0)
          polygon.reset(transformation.transformGeometry(*polygon));
        all_circles.push_back(polygon);
      }
    }

    // Generate a <g> group of labels

    if (!labels.empty())
    {
      std::string labels_iri = theState.makeQid("circle-labels-");
      if (!theState.addId(labels_iri))
        throw Fmi::Exception(BCP, "Non-unique ID assigned to circle radius labels")
            .addParameter("ID", labels_iri);

      CTPP::CDT labels_cdt(CTPP::CDT::HASH_VAL);
      labels_cdt["attributes"]["id"] = labels_iri;
      labels_cdt["start"] = "<g";
      labels_cdt["end"] = "";
      theState.addAttributes(theGlobals, labels_cdt, labels.attributes);
      theLayersCdt.PushBack(labels_cdt);

      const std::map<std::string, double> directions = {
          {"north", 90}, {"south", -90}, {"west", -180}, {"east", 180}};

      auto current_circle = all_circles.begin();
      for (const auto& location : locations)
      {
        auto lon = location->longitude;
        auto lat = location->latitude;
        for (const auto& circle : circles)
        {
          auto txt = labels.format(circle.radius);

          for (const auto& direction : labels.layout)
          {
            double x = 0;
            double y = 0;

            auto dir = directions.find(direction);
            if (dir != directions.end())
            {
              auto angle = dir->second;
              auto lonlat = coordinate_at_distance(lon, lat, angle, circle.radius * 1000);

              x = lonlat.first;
              y = lonlat.second;
              transformation.transform(x, y);  // latlon -> world xy
            }
            else
            {
              if (direction == "top")
                find_top_point(x, y, *current_circle);
              else if (direction == "bottom")
                find_bottom_point(x, y, *current_circle);
              else if (direction == "left")
                find_left_point(x, y, *current_circle);
              else if (direction == "right")
                find_right_point(x, y, *current_circle);
            }

            if (clipbox.position(x, y) == Fmi::Box::Inside)
            {
              box.transform(x, y);  // world xy -> pixel

              auto xpos = lround(x + labels.dx);
              auto ypos = lround(y + labels.dy);

              CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
              text_cdt["start"] = " <text";
              text_cdt["end"] = "</text>";
              text_cdt["cdata"] = txt;

              text_cdt["attributes"]["x"] = Fmi::to_string(xpos);
              text_cdt["attributes"]["y"] = Fmi::to_string(ypos);

              theState.addAttributes(theGlobals, text_cdt, labels.textattributes);
              theLayersCdt.PushBack(text_cdt);
            }
          }
          ++current_circle;
        }
      }
      // Close labels_cdt
      theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
    }

    // Generate a <g> group of circles

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Common attributes
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Process one location at a time

    const auto precision = theState.getPrecision("circle");

    auto current_circle = all_circles.begin();
    for (const auto& location : locations)
    {
      auto lon = location->longitude;
      auto lat = location->latitude;
      for (const auto& circle : circles)
      {
        auto geom = *current_circle;
        ++current_circle;

        if (!geom || geom->IsEmpty() != 0)
          continue;

        if (lines)
          geom.reset(Fmi::OGR::lineclip(*geom, clipbox));
        else
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
    Fmi::hash_combine(hash, Fmi::hash_value(geoids));
    for (const auto& circle : circles)
      Fmi::hash_combine(hash, circle.hash_value(theState));
    Fmi::hash_combine(hash, labels.hash_value(theState));
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
