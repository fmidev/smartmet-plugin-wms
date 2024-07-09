#include "MapLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include "StyleSheet.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <macgyver/Exception.h>
#include <timeseries/ParameterFactory.h>

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

void MapLayer::init(Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Map JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("map");

    // Extract all members

    JsonTools::remove_double(precision, theJson, "precision");

    auto json = JsonTools::remove(theJson, "map");
    map.init(json, theConfig);

    json = JsonTools::remove(theJson, "styles");
    if (!json.isNull())
      (styles = MapStyles())->init(json, theConfig);
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

void MapLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "MapLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Get projection details

    bool has_data_proj = (projection.crs && *projection.crs == "data");

    // Update projection from querydata if necessary
    if (has_data_proj)
    {
      if (!source || *source != "grid")
      {
        // Establish the data
        auto q = getModel(theState);
        projection.update(q);
      }
      else
      {
        return;
      }
    }

    if (!styles)
      generate_full_map(theGlobals, theLayersCdt, theState);
    else
      generate_styled_map(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a map with no feature specific styling
 */
// ----------------------------------------------------------------------

void MapLayer::generate_full_map(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    const auto& crs = projection.getCRS();
    const Fmi::Box& box = projection.getBox();

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Fetch the shape in our projection

    OGRGeometryPtr geom;

    const auto& gis = theState.getGisEngine();
    {
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "getShape finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }
      geom = gis.getShape(&crs, map.options);

      if (!geom)
      {
        std::string msg =
            "Requested map data is empty: '" + map.options.schema + '.' + map.options.table + "'";
        if (map.options.minarea)
          msg += " Is the minarea limit too large?";

        throw Fmi::Exception(BCP, msg);
      }
    }

    // Clip it

    {
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "polyclip finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }
      if (map.lines)
        geom.reset(Fmi::OGR::lineclip(*geom, clipbox));  // fast and hence not cached in gisengine
      else
        geom.reset(Fmi::OGR::polyclip(*geom, clipbox));  // fast and hence not cached in gisengine
    }

    // We might zoom in so close that some geometry becomes invisible - just don't generate anything
    if (!geom)
      return;

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    std::string iri = qid;

    // Store the path. Note that we allow duplicate IDs since multiple views may
    // refer to the same map.

    {
      std::string arcNumbers;
      std::string arcCoordinates;
      std::string pointCoordinates;

      CTPP::CDT object_cdt;
      std::string objectKey = "map:" + qid;
      object_cdt["objectKey"] = objectKey;

      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "Generating coordinate data finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }

      CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
      map_cdt["iri"] = iri;
      map_cdt["type"] = Geometry::name(*geom, theState.getType());
      map_cdt["layertype"] = "map";

      pointCoordinates = Geometry::toString(*geom,
                                            theState.getType(),
                                            box,
                                            crs,
                                            precision,
                                            theState.arcHashMap,
                                            theState.arcCounter,
                                            arcNumbers,
                                            arcCoordinates);

      if (!pointCoordinates.empty())
        map_cdt["data"] = pointCoordinates;

      theState.addPresentationAttributes(map_cdt, css, attributes);

      if (theState.getType() == "topojson")
      {
        if (!arcNumbers.empty())
          map_cdt["arcs"] = arcNumbers;

        object_cdt["paths"][iri] = map_cdt;

        if (!arcCoordinates.empty())
        {
          CTPP::CDT arc_cdt(CTPP::CDT::HASH_VAL);
          arc_cdt["data"] = arcCoordinates;
          theGlobals["arcs"][theState.insertCounter] = arc_cdt;
          theState.insertCounter++;
        }

        theGlobals["objects"][objectKey] = object_cdt;
        if (precision >= 1.0)
          theGlobals["precision"] = pow(10.0, -(int)precision);
      }
      else
      {
        theGlobals["paths"][iri] = map_cdt;
      }
    }

    // Do not produce a use-statement for empty data or in the header
    if (geom->IsEmpty() == 0)
    {
      // Update the globals

      theGlobals["bbox"] = std::to_string(box.xmin()) + "," + std::to_string(box.ymin()) + "," +
                           std::to_string(box.xmax()) + "," + std::to_string(box.ymax());

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a map with feature specific styling
 */
// ----------------------------------------------------------------------

void MapLayer::generate_styled_map(CTPP::CDT& theGlobals,
                                   CTPP::CDT& theLayersCdt,
                                   const State& theState)
{
  try
  {
    if (!producer)
      throw Fmi::Exception(BCP, "MapLayer producer not set for styling");

    if (theState.isObservation(producer))
      throw Fmi::Exception(BCP, "Observations not supported in MapLayer")
          .addParameter("producer", *producer);

    // Establish data

    auto q = getModel(theState);

    if (!q)
      throw Fmi::Exception(BCP, "MapLayer querydata undefined");

    if (q->isGrid())
      throw Fmi::Exception(BCP, "MapLayer querydata must be point data, not gridded");

    // Set the parameter and time

    if (styles->parameter.empty())
      throw Fmi::Exception(BCP, "MapLayer styling parameter not set");
    auto param = TS::ParameterFactory::instance().parse(styles->parameter);

    if (!q->param(param.number()))
      throw Fmi::Exception(BCP, "MapLayer parameter not available in querydata")
          .addParameter("parameter", styles->parameter);

    auto valid_time = getValidTime();
    if (!q->time(valid_time))
      throw Fmi::Exception(BCP, "Selected MapLayer time not available in querydata")
          .addParameter("time", Fmi::to_iso_string(valid_time));

    // Establish projection, bbox and optional clipping bbox

    const auto& crs = projection.getCRS();
    const Fmi::Box& box = projection.getBox();

    const auto clipbox = getClipBox(box);

    // Fetch the features in our projection

    Fmi::Features features;

    const auto& gis = theState.getGisEngine();
    {
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "getFeatures finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }

      map.options.fieldnames.insert(styles->field);
      features = gis.getFeatures(crs, map.options);

      for (const auto& feature : features)
      {
        if (!feature->geom || feature->geom->IsEmpty() != 0)
        {
          std::string msg =
              "Requested map data is empty: '" + map.options.schema + '.' + map.options.table + "'";
          if (map.options.minarea)
            msg += " Is the minarea limit too large?";
          throw Fmi::Exception(BCP, msg);
        }
      }
    }

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Handle each feature separately

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    int counter = 0;

    for (const auto& feature : features)
    {
      if (feature->geom == nullptr || feature->geom->IsEmpty() == 1)
        continue;

      // Clip the geometry
      OGRGeometryPtr geom = feature->geom;
      if (map.lines)
        geom.reset(Fmi::OGR::lineclip(*geom, clipbox));
      else
        geom.reset(Fmi::OGR::polyclip(*geom, clipbox));  // fast and hence not cached in gisengine

      if (!geom)
        continue;

      // Increase counter for each feature

      std::string iri = qid + Fmi::to_string(++counter);

      CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
      map_cdt["iri"] = iri;
      map_cdt["type"] = Geometry::name(*geom, theState.getType());
      map_cdt["layertype"] = "map";
      map_cdt["data"] = Geometry::toString(*geom, theState.getType(), box, crs, precision);

      theGlobals["paths"][iri] = map_cdt;

      // Establish station to pick from querydata

      std::optional<std::string> station_name;
      int station_number = -1;

      const auto& feature_value =
          feature->attributes.at(styles->field);  // <int,double,string,time>

      if (styles->features.empty())
      {
        // Assume feature value matches station number
        if (feature_value.index() == 0)
          station_number = std::get<int>(feature_value);
        else
          throw Fmi::Exception(BCP, "Feature type for a styled MapLayer must be int or string");
      }
      else
      {
        auto station_pos = styles->features.end();

        if (feature_value.index() == 2)
          station_pos = styles->features.find(std::get<std::string>(feature_value));
        else if (feature_value.index() == 0)
          station_pos = styles->features.find(Fmi::to_string(std::get<int>(feature_value)));
        else
          throw Fmi::Exception(BCP, "Feature type for a styled MapLayer must be int or string");

        if (station_pos == styles->features.end())
          continue;
        station_name = station_pos->first;
        station_number = station_pos->second;
      }

      auto info = q->info();
      if (!info->Location(station_number))
        continue;

      auto value = info->FloatValue();
      if (value == kFloatMissing)
        continue;

      // Do not produce a use-statement for empty data or in the header
      if (geom->IsEmpty() == 0)
      {
        // Add the SVG use element
        CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
        tag_cdt["start"] = "<use";
        tag_cdt["end"] = "/>";

        // Cannot call addAttributes two times or style will be messed up, better
        // create one Attributes object and add everything once
        Attributes attrs;
        attrs.add("xlink:href", "#" + iri);

        // Add feature specific attributes
        if (!styles->features.empty() && station_name)
        {
          const auto& feature_attrs = styles->feature_attributes.find(*station_name);
          if (feature_attrs != styles->feature_attributes.end())
            attrs.add(feature_attrs->second);
        }

        // Add data specific attributes. These will overide feature specific settings
        auto selection = Select::attribute(styles->data_attributes, value);
        if (selection)
          attrs.add(selection->attributes);

        // And this will generate a single style-setting joining all styles set above
        theState.addAttributes(theGlobals, tag_cdt, attrs);

        // Tag to group
        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    Fmi::hash_combine(hash, Dali::hash_value(map, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Dali::hash_value(styles, theState));

    // Add data hash
    if (styles && !theState.isObservation(producer))
      Fmi::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

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
