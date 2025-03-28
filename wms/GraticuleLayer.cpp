#include "GraticuleLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <spine/Json.h>
#include <ogr_geometry.h>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{

const std::set<std::string> valid_label_layouts = {
    "none", "edges", "grid", "center", "left_bottom"};

const std::set<std::string> valid_line_layouts = {"grid", "ticks"};

using Graticule = GraticuleLayer::Graticule;

struct GraticuleLines
{
  std::unique_ptr<OGRGeometry> labeled;
  std::unique_ptr<OGRGeometry> unlabeled;
};

bool is_labeled(int value, const Graticule& graticule)
{
  if (graticule.labels.layout == "none")
    return false;
  return (value % graticule.labels.step == 0);
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate geographic graticule lines
 */
// ----------------------------------------------------------------------

GraticuleLines generate_geographic_lines(const Graticule& g)
{
  try
  {
    auto labeled = std::make_unique<OGRMultiLineString>();
    auto unlabeled = std::make_unique<OGRMultiLineString>();

    // 0.5 degrees is usually good enough for meteorological data, 1.0 degrees may
    // produce ~1 km errors
    const double linestep = 0.5;

    // Add vertical lines
    for (int lon = -180; lon <= 180; lon += g.step)
    {
      auto* line = new OGRLineString();
      for (int i = 0;; i++)
      {
        auto lat = -90.0 + i * linestep;
        if (lat > 90)
          break;
        line->addPoint(lon, lat);
      }

      if (is_labeled(lon, g))
        labeled->addGeometryDirectly(line);
      else
        unlabeled->addGeometryDirectly(line);
    }

    // Add horizontal lines
    for (int lat = -90; lat <= 90; lat += g.step)
    {
      auto* line = new OGRLineString();
      for (int i = 0;; i++)
      {
        auto lon = -180 + i * linestep;
        if (lon > 180)
          break;
        line->addPoint(lon, lat);
      }

      if (is_labeled(lat, g))
        labeled->addGeometryDirectly(line);
      else
        unlabeled->addGeometryDirectly(line);
    }

    labeled->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());
    unlabeled->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());

    GraticuleLines glines;
    if (labeled->IsEmpty() == 0)
      glines.labeled = std::move(labeled);
    if (unlabeled->IsEmpty() == 0)
      glines.unlabeled = std::move(unlabeled);

    return glines;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule lines in image coordinates
 */
// ----------------------------------------------------------------------

GraticuleLines generate_grid(const Graticule& g,
                             const Fmi::SpatialReference& crs,
                             const Fmi::Box& box)
{
  try
  {
    // Global geographic grid

    auto glines = generate_geographic_lines(g);

    // Projected grid with possible interrupt cuts done

    Fmi::CoordinateTransformation transformation("WGS84", crs);
    glines.labeled.reset(transformation.transformGeometry(*glines.labeled));
    glines.unlabeled.reset(transformation.transformGeometry(*glines.unlabeled));

    return glines;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule ticks in image coordinates
 *
 * We need to find out for the bounding box coordinates where they intersect
 * with the desired meridians etc, and then produce small ticks at those
 * coordinates.
 */
// ----------------------------------------------------------------------

GraticuleLines generate_ticks(const Graticule& g,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box)
{
  try
  {
    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule lines in image coordinates
 */
// ----------------------------------------------------------------------

GraticuleLines generate_lines(const Graticule& g,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box)
{
  try
  {
    GraticuleLines glines;

    if (g.layout == "grid")
      glines = generate_grid(g, crs, box);
    else if (g.layout == "ticks")
      glines = generate_ticks(g, crs, box);
    else
      throw Fmi::Exception(BCP, "Invalid graticule layout").addParameter("layout", g.layout);

    glines.labeled.reset(Fmi::OGR::lineclip(*glines.labeled, box));
    glines.unlabeled.reset(Fmi::OGR::lineclip(*glines.unlabeled, box));
    return glines;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Generate a single Graticule definition
 */
// ----------------------------------------------------------------------

Graticule remove_graticule(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "graticules settings must be JSON objects");

    Graticule g;
    JsonTools::remove_string(g.layout, theJson, "layout");
    JsonTools::remove_uint(g.step, theJson, "step");
    JsonTools::remove_uint(g.length, theJson, "length");
    auto json = JsonTools::remove(theJson, "attributes");
    g.attributes.init(json, theConfig);

    // exceptions to the given step
    auto json = JsonTools::remove(theJson, "except");
    if (json.isArray())
    {
      for (const auto& num : json)
      {
        if (num.isUInt())
          g.except.push_back(num.asUInt());
        else
          throw Fmi::Exception(BCP,
                               "graticule except setting must be an array of unsigned integers");
      }
    }
    else if (json.isUInt())
      g.except.push_back(json.asUInt());
    else
      throw Fmi::Exception(BCP, "graticule except setting must be an array of unsigned integers");

    // label rendering

    json = JsonTools::remove(theJson, "labels");
    auto& l = g.labels;  // shorthand

    if (!json.isNull())  // labels are not obligatory
    {
      if (!json.isObject())
        throw Fmi::Exception(BCP, "graticule labels setting must be a JSON object");

      JsonTools::remove_string(l.layout, json, "layout");
      JsonTools::remove_uint(l.step, json, "step");
      JsonTools::remove_bool(l.upright, json, "upright");
      JsonTools::remove_bool(l.degree_sign, json, "degree_sign");
      JsonTools::remove_bool(l.minus_sign, json, "minus_sign");
      JsonTools::remove_int(l.dx, json, "dx");
      JsonTools::remove_int(l.dy, json, "dy");
      json = JsonTools::remove(json, "attributes");
      l.attributes.init(json, theConfig);
    }

    // Sanity checks
    if (valid_line_layouts.find(g.layout) == valid_line_layouts.end())
      throw Fmi::Exception(BCP, "graticule layout must be grid|ticks")
          .addParameter("layout", g.layout);

    if (g.step < 1)
      throw Fmi::Exception(BCP, "graticule step must be at least 1")
          .addParameter("step", Fmi::to_string(g.step));

    if (180 % g.step != 0)
      throw Fmi::Exception(BCP, "graticule step should divide 180 evenly")
          .addParameter("step", Fmi::to_string(g.step));

    if (g.layout == "ticks" && g.length < 1)
      throw Fmi::Exception(BCP, "graticule tick length must be at least 1");

    if (valid_label_layouts.find(l.layout) == valid_label_layouts.end())
      throw Fmi::Exception(BCP,
                           "graticule labels layout must be none|edges|grid|center|left_bottom")
          .addParameter("layout", l.layout);

    if (l.layout != "none")
    {
      if (l.step < 1)
        throw Fmi::Exception(BCP, "graticule label step must be at least 1")
            .addParameter("step", Fmi::to_string(l.step));

      if (180 % l.step != 0)
        throw Fmi::Exception(BCP, "graticule label step should divide 180 evenly")
            .addParameter("step", Fmi::to_string(l.step));
    }

    return g;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize graticule definitions
 */
// ----------------------------------------------------------------------

void GraticuleLayer::init(Json::Value& theJson,
                          const State& theState,
                          const Config& theConfig,
                          const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Gratitule-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("graticule");

    // Extract member values

    JsonTools::remove_string(mask, theJson, "mask");
    JsonTools::remove_string(mask_id, theJson, "mask_id");
    JsonTools::remove_double(precision, theJson, "precision");

    auto json = JsonTools::remove(theJson, "graticules");

    if (json.isNull())
      throw Fmi::Exception(BCP, "Graticule-layer must define at least one graticule");

    if (json.isObject())
      graticules.emplace_back(remove_graticule(json, theConfig));
    else if (json.isArray())
    {
      for (auto& gjson : json)
        graticules.emplace_back(remove_graticule(gjson, theConfig));
    }
    else
      throw Fmi::Exception(
          BCP,
          "graticules setting must be a single graticule definition or an array of definitions");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate data for content formatting
 *
 * We first generate the numbers for all graticules in the order they are listed.
 * Then if a mask is enabled, a mask layer will be generated based on the numbers.
 * The graticule lines are added last. The layer itself will be placed into
 * <g>...</g> with the main level attributes, and the numbers will be placed
 * into an inner block with label formatting attributes.
 */
// ----------------------------------------------------------------------

void GraticuleLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // Ignore formats not really meant for retrieving graticules

    if (theState.getType() == "topojson" || theState.getType() == "geojson")
      return;

    // Establish projection details

    auto q = getModel(theState);
    if (q)
      projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();
    const auto clipbox = getClipBox(box);

    // First generate the lines, and clip them to the image area

    std::vector<GraticuleLines> all_glines;
    for (const auto& graticule : graticules)
    {
      auto glines = generate_lines(graticule, crs, box);
      glines.labeled.reset(Fmi::OGR::lineclip(*glines.labeled, clipbox));
      glines.unlabeled.reset(Fmi::OGR::lineclip(*glines.unlabeled, clipbox));
      all_glines.emplace_back(std::move(glines));
    }

    // Handle CSS

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Clip if necessary
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Begin by generating a containing group

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Add optional numbers to the containing group

    // Add optional mask to the containing group

    // Add lines to the containing group, possibly masked

    CTPP::CDT lines_cdt(CTPP::CDT::HASH_VAL);
    lines_cdt["start"] = "<g";
    lines_cdt["end"] = "</g>";

    // TODO: Add label mask to the group!

    for (auto i = 0u; i < graticules.size(); i++)
    {
      const auto& graticule = graticules.at(i);
      const auto& glines = all_glines.at(i);

      if ((!glines.labeled || glines.labeled->IsEmpty() == 1) &&
          (!glines.unlabeled || glines.unlabeled->IsEmpty() == 1))
        continue;

      CTPP::CDT graticule_cdt(CTPP::CDT::HASH_VAL);

      // Store the path with unique QID
      std::string iri = qid + (qid.empty() ? "" : ".") + theState.makeQid("graticule");

      if (!theState.addId(iri))
        throw Fmi::Exception(BCP, "Non-unique ID assigned to graticule").addParameter("ID", iri);

      graticule_cdt["iri"] = iri;
      graticule_cdt["layertype"] = "graticule";

      auto geom = std::make_unique<OGRGeometryCollection>();
      if (glines.labeled && glines.labeled->IsEmpty() == 0)
        geom->addGeometry(glines.labeled.get());
      if (glines.unlabeled && glines.unlabeled->IsEmpty() == 0)
        geom->addGeometry(glines.unlabeled.get());

      std::string arcNumbers;
      std::string arcCoordinates;
      std::string pointCoordinates = Geometry::toString(*geom,
                                                        theState.getType(),
                                                        box,
                                                        crs,
                                                        precision,
                                                        theState.arcHashMap,
                                                        theState.arcCounter,
                                                        arcNumbers,
                                                        arcCoordinates);

      if (!pointCoordinates.empty())
        graticule_cdt["data"] = pointCoordinates;

      theState.addPresentationAttributes(graticule_cdt, css, attributes, graticule.attributes);
      theGlobals["paths"][iri] = graticule_cdt;

      // Add the SVG use element
      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";
      theState.addAttributes(theGlobals, tag_cdt, graticule.attributes);
      tag_cdt["attributes"]["xlink:href"] = "#" + iri;
      graticule_cdt["tags"].PushBack(tag_cdt);

      theLayersCdt.PushBack(graticule_cdt);
    }

    // Finish the containing layer
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the graticule definitions
 */
// ----------------------------------------------------------------------

std::size_t GraticuleLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    for (const auto& g : graticules)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(g.layout));
      Fmi::hash_combine(hash, Fmi::hash_value(g.step));
      Fmi::hash_combine(hash, Fmi::hash_value(g.except));
      Fmi::hash_combine(hash, Fmi::hash_value(g.length));
      Fmi::hash_combine(hash, g.attributes.hash_value(theState));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.layout));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.step));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.upright));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.degree_sign));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.minus_sign));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.dx));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.dy));
      Fmi::hash_combine(hash, g.labels.attributes.hash_value(theState));
    }
    Fmi::hash_combine(hash, Fmi::hash_value(mask));
    Fmi::hash_combine(hash, Fmi::hash_value(mask_id));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));

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
