//======================================================================

#include "LocationLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/geonames/Engine.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/NearTree.h>
#include <spine/Exception.h>
#include <spine/Json.h>
#include <ogr_spatialref.h>

namespace SmartMet
{
class XY
{
 public:
  XY(double theX, double theY) : itsX(theX), itsY(theY) {}
  double x() const { return itsX; }
  double y() const { return itsY; }

 private:
  double itsX;
  double itsY;
};

namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void LocationLayer::init(const Json::Value& theJson,
                         const State& theState,
                         const Config& theConfig,
                         const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Location-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("keyword", nulljson);
    if (!json.isNull())
      keyword = json.asString();

    json = theJson.get("mindistance", nulljson);
    if (!json.isNull())
      mindistance = json.asDouble();

    json = theJson.get("countries", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_set("countries", countries, json);

    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      symbol = json.asString();

    json = theJson.get("symbols", nulljson);
    if (!json.isNull())
    {
      if (json.isArray())
      {
        // Just a default selection is given
        std::vector<AttributeSelection> selection;
        Spine::JSON::extract_array("symbols", selection, json, theConfig);
        symbols["default"] = selection;
      }
      else if (json.isObject())
      {
        const auto features = json.getMemberNames();
        for (const auto& feature : features)
        {
          const Json::Value& innerjson = json[feature];
          if (!innerjson.isArray())
            throw Spine::Exception(
                BCP,
                "LocationLayer symbols setting does not contain a hash of JSON arrays for each "
                "feature");
          std::vector<AttributeSelection> selection;
          Spine::JSON::extract_array("symbols", selection, innerjson, theConfig);
          symbols[feature] = selection;
        }
      }
      else
        throw Spine::Exception(BCP,
                               "LocationLayer symbols setting must be an array or a JSON hash");
    }
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

void LocationLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Time execution

    std::string report = "LocationLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // A keyword must be defined

    if (keyword.empty())
      throw Spine::Exception(BCP, "No keyword specified for location-layer");

    // A symbol must be defined either globally or for values

    if (!symbol && symbols.empty())
      throw Spine::Exception(
          BCP,
          "Must define default symbol with 'symbol' or several 'symbols' for specific values in a "
          "location-layer");

    // Establish the data
    auto q = getModel(theState);

    // Get projection details
    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Create the coordinate transformation from geonames coordinates to image coordinates

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Read the data from geonames. We use autocomplete mode
    // to get a good prioritized sort for the locations.
    // Use even more query options?

    const auto& engine = theState.getGeoEngine();
    Locus::QueryOptions options;
    auto locations = engine.keywordSearch(options, keyword);
    engine.sort(locations);

    if (locations.empty())
      throw Spine::Exception(BCP, "No locations found for keyword '" + keyword + "'");

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the symbols
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Maintain a tree of selected points for removing candidates too close to them

    Fmi::NearTree<XY> selected_coordinates;

    // Process all the locations

    for (const auto& location : locations)
    {
      // The location information
      double lon = location->longitude;
      double lat = location->latitude;

      // Project it to pixel coordinates

      double x = lon;
      double y = lat;
      transformation.Transform(x, y);

      // Skip locations outside the image.

      if (!inside(box, x, y))
        continue;

      box.transform(x, y);

#if 0
        std::cerr << location->name
            << " at "
            << lon << "," << lat
            << " --> "
            << x << "," << y
            << " priority " << location->priority << std::endl;
#endif

      // Skip the location if it is too close to previous points
      XY xy(x, y);
      if (mindistance > 0)
      {
        auto match = selected_coordinates.nearest(xy, mindistance);
        if (match)
          continue;
        selected_coordinates.insert(xy);
      }

      // Start generating the hash

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      std::string iri;

      if (symbol)
        iri = *symbol;

      if (!symbols.empty())
      {
        auto feature_selection = symbols.find(location->feature);
        if (feature_selection == symbols.end())
          feature_selection = symbols.find("default");
        if (feature_selection != symbols.end())
        {
          auto selection = Select::attribute(feature_selection->second, location->population);
          if (selection)
          {
            if (selection->symbol)
              iri = *selection->symbol;
            theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
          }
        }
      }

      if (!iri.empty())
      {
        if (theState.addId(iri))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        tag_cdt["attributes"]["xlink:href"] = "#" + iri;
        tag_cdt["attributes"]["x"] = Fmi::to_string(lround(x));
        tag_cdt["attributes"]["y"] = Fmi::to_string(lround(y));

        group_cdt["tags"].PushBack(tag_cdt);
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

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t LocationLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Dali::hash_combine(hash, Dali::hash_value(keyword));
    Dali::hash_combine(hash, Dali::hash_value(mindistance));
    Dali::hash_combine(hash, Dali::hash_value(countries));
    Dali::hash_combine(hash, Dali::hash_value(symbol));
    Dali::hash_combine(hash, Dali::hash_symbol(symbol, theState));

    for (const auto& name_symbol : symbols)
    {
      Dali::hash_combine(hash, Dali::hash_value(name_symbol.first));
      for (const auto& selection : name_symbol.second)
      {
        Dali::hash_combine(hash, Dali::hash_value(selection, theState));
      }
    }
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
