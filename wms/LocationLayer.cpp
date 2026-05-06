//======================================================================

#include "LocationLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "LabelPlacement.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include "TextUtility.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/geonames/Engine.h>
#include <engines/gis/Engine.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/NearTree.h>
#include <macgyver/StringConversion.h>
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

// ======================================================================
// Local helpers for parsing LabelConfig from JSON
// ======================================================================

namespace
{

LabelConfig parseLabelConfig(Json::Value& json, const Config& /*config*/)
{
  LabelConfig cfg;

  if (json.isNull())
    return cfg;

  if (!json.isObject())
    throw Fmi::Exception(BCP, "LocationLayer 'label' setting must be a JSON object");

  // Algorithm
  std::string algo_str = "none";
  JsonTools::remove_string(algo_str, json, "algorithm");
  if (algo_str == "none")
    cfg.algorithm = PlacementAlgorithm::None;
  else if (algo_str == "fixed")
    cfg.algorithm = PlacementAlgorithm::Fixed;
  else if (algo_str == "greedy")
    cfg.algorithm = PlacementAlgorithm::Greedy;
  else if (algo_str == "priority-greedy")
    cfg.algorithm = PlacementAlgorithm::PriorityGreedy;
  else if (algo_str == "simulated-annealing")
    cfg.algorithm = PlacementAlgorithm::SimulatedAnnealing;
  else
    throw Fmi::Exception(BCP, "Unknown label placement algorithm '" + algo_str + "'");

  // Geometry
  JsonTools::remove_int(cfg.candidates, json, "candidates");
  JsonTools::remove_double(cfg.offset, json, "offset");

  // Style
  JsonTools::remove_string(cfg.font_family, json, "font_family");
  JsonTools::remove_double(cfg.font_size, json, "font_size");
  JsonTools::remove_string(cfg.font_weight, json, "font_weight");
  JsonTools::remove_string(cfg.fill, json, "fill");
  JsonTools::remove_string(cfg.stroke, json, "stroke");
  JsonTools::remove_double(cfg.stroke_width, json, "stroke_width");
  JsonTools::remove_double(cfg.stroke_opacity, json, "stroke_opacity");
  JsonTools::remove_int(cfg.max_labels, json, "max_labels");

  // Fixed-specific
  JsonTools::remove_string(cfg.fixed_position, json, "fixed_position");

  // SA sub-object
  auto sa_json = JsonTools::remove(json, "simulated_annealing");
  if (!sa_json.isNull())
  {
    if (!sa_json.isObject())
      throw Fmi::Exception(BCP, "LocationLayer 'label.simulated_annealing' must be an object");
    JsonTools::remove_int(cfg.sa_iterations, sa_json, "iterations");
    JsonTools::remove_double(cfg.sa_initial_temp, sa_json, "initial_temperature");
    JsonTools::remove_double(cfg.sa_cooling_rate, sa_json, "cooling_rate");
    JsonTools::remove_double(cfg.sa_overlap_weight, sa_json, "overlap_weight");
    JsonTools::remove_double(cfg.sa_position_weight, sa_json, "position_weight");
  }

  // Population-based style classes
  auto classes_json = JsonTools::remove(json, "classes");
  if (!classes_json.isNull())
  {
    if (!classes_json.isArray())
      throw Fmi::Exception(BCP, "LocationLayer 'label.classes' must be a JSON array");
    for (const auto& cls_json : classes_json)
    {
      LabelStyleClass cls;
      if (cls_json.isMember("lolimit"))
        cls.lolimit = cls_json["lolimit"].asDouble();
      if (cls_json.isMember("hilimit"))
        cls.hilimit = cls_json["hilimit"].asDouble();
      if (cls_json.isMember("font_size"))
        cls.font_size = cls_json["font_size"].asDouble();
      if (cls_json.isMember("font_weight"))
        cls.font_weight = cls_json["font_weight"].asString();
      if (cls_json.isMember("fill"))
        cls.fill = cls_json["fill"].asString();
      cfg.classes.push_back(cls);
    }
  }

  return cfg;
}

std::size_t hashLabelConfig(const LabelConfig& cfg)
{
  std::size_t hash = 0;
  Fmi::hash_combine(hash, Fmi::hash_value(static_cast<int>(cfg.algorithm)));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.candidates));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.offset));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.font_family));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.font_size));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.font_weight));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.fill));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.stroke));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.stroke_width));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.stroke_opacity));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.max_labels));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.fixed_position));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.sa_iterations));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.sa_initial_temp));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.sa_cooling_rate));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.sa_overlap_weight));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.sa_position_weight));
  for (const auto& cls : cfg.classes)
  {
    if (cls.lolimit) Fmi::hash_combine(hash, Fmi::hash_value(*cls.lolimit));
    if (cls.hilimit) Fmi::hash_combine(hash, Fmi::hash_value(*cls.hilimit));
    Fmi::hash_combine(hash, Fmi::hash_value(cls.font_size));
    Fmi::hash_combine(hash, Fmi::hash_value(cls.font_weight));
    Fmi::hash_combine(hash, Fmi::hash_value(cls.fill));
  }
  return hash;
}

// Emit a <text> SVG element (and optional halo) as top-level layers.
// Text content must travel via layer.cdata, which the SVG template only
// renders at the layer level — nested entries in layer.tags do not get
// their cdata expanded, see NumberLayer.cpp for the same pattern.
void emitText(CTPP::CDT& theLayersCdt,
              const PlacedLabel& pl,
              const LabelConfig& cfg,
              double x, double y)
{
  // Halo / stroke pass (renders first → visually behind fill text)
  if (cfg.stroke_width > 0 && !cfg.stroke.empty())
  {
    CTPP::CDT halo(CTPP::CDT::HASH_VAL);
    halo["start"] = "<text";
    halo["end"] = "</text>";
    halo["cdata"] = pl.text;
    halo["attributes"]["x"] = Fmi::to_string(lround(x));
    halo["attributes"]["y"] = Fmi::to_string(lround(y));
    halo["attributes"]["font-family"] = cfg.font_family;
    halo["attributes"]["font-size"] = Fmi::to_string(pl.font_size);
    if (pl.font_weight != "normal")
      halo["attributes"]["font-weight"] = pl.font_weight;
    halo["attributes"]["stroke"] = cfg.stroke;
    halo["attributes"]["stroke-width"] = Fmi::to_string(cfg.stroke_width);
    halo["attributes"]["stroke-opacity"] = Fmi::to_string(cfg.stroke_opacity);
    halo["attributes"]["stroke-linejoin"] = "round";
    halo["attributes"]["fill"] = "none";
    halo["attributes"]["xml:space"] = "preserve";
    theLayersCdt.PushBack(halo);
  }

  // Fill / foreground pass
  CTPP::CDT text(CTPP::CDT::HASH_VAL);
  text["start"] = "<text";
  text["end"] = "</text>";
  text["cdata"] = pl.text;
  text["attributes"]["x"] = Fmi::to_string(lround(x));
  text["attributes"]["y"] = Fmi::to_string(lround(y));
  text["attributes"]["font-family"] = cfg.font_family;
  text["attributes"]["font-size"] = Fmi::to_string(pl.font_size);
  if (pl.font_weight != "normal")
    text["attributes"]["font-weight"] = pl.font_weight;
  text["attributes"]["fill"] = pl.fill;
  text["attributes"]["xml:space"] = "preserve";
  theLayersCdt.PushBack(text);
}

}  // anonymous namespace

// ======================================================================
// LocationLayer::init
// ======================================================================

void LocationLayer::init(Json::Value& theJson,
                         const State& theState,
                         const Config& theConfig,
                         const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Location-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(keyword, theJson, "keyword");
    JsonTools::remove_double(mindistance, theJson, "mindistance");

    auto json = JsonTools::remove(theJson, "countries");
    if (!json.isNull())
      JsonTools::extract_set("countries", countries, json);

    JsonTools::remove_string(symbol, theJson, "symbol");

    json = JsonTools::remove(theJson, "symbols");
    if (!json.isNull())
    {
      if (json.isArray())
      {
        // Just a default selection is given
        std::vector<AttributeSelection> selection;
        JsonTools::extract_array("symbols", selection, json, theConfig);
        symbols["default"] = selection;
      }
      else if (json.isObject())
      {
        const auto features = json.getMemberNames();
        for (const auto& feature : features)
        {
          Json::Value& innerjson = json[feature];
          if (!innerjson.isArray())
            throw Fmi::Exception(
                BCP,
                "LocationLayer symbols setting does not contain a hash of JSON arrays for each "
                "feature");
          std::vector<AttributeSelection> selection;
          JsonTools::extract_array("symbols", selection, innerjson, theConfig);
          symbols[feature] = selection;
        }
      }
      else
        throw Fmi::Exception(BCP, "LocationLayer symbols setting must be an array or a JSON hash");
    }

    // Parse optional label placement config
    auto label_json = JsonTools::remove(theJson, "label");
    if (!label_json.isNull())
      label_config = parseLabelConfig(label_json, theConfig);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ======================================================================
// LocationLayer::generate
// ======================================================================

void LocationLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Time execution
    std::string report = "LocationLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // A keyword must be defined
    if (keyword.empty())
      throw Fmi::Exception(BCP, "No keyword specified for location-layer");

    // A symbol must be defined either globally or for values
    if (!symbol && symbols.empty())
      throw Fmi::Exception(
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

    // Read the data from geonames. We use autocomplete mode to get a good
    // prioritized sort for the locations.
    const auto& engine = theState.getGeoEngine();
    Locus::QueryOptions options;
    auto locations = engine.keywordSearch(options, keyword);
    engine.sort(locations);

    if (locations.empty())
      throw Fmi::Exception(BCP, "No locations found for keyword '" + keyword + "'");

    // Clip if necessary
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // ----------------------------------------------------------------
    // PASS 1: filter by mindistance, collect accepted locations and
    //         their pixel coordinates.  This decouples the NearTree
    //         filtering from symbol/label emission so label placement
    //         can see the full set before any SVG is written.
    // ----------------------------------------------------------------

    // Store only the fields needed for symbol selection and label placement;
    // this avoids taking a type dependency on the internal location list type.
    struct AcceptedLocation
    {
      double x = 0;
      double y = 0;
      std::string name;
      std::string feature;
      int population = 0;
    };

    std::vector<AcceptedLocation> accepted;

    Fmi::NearTree<XY> selected_coordinates;

    for (const auto& location : locations)
    {
      double x = location->longitude;
      double y = location->latitude;
      transformation.transform(x, y);
      box.transform(x, y);

      if (!inside(box, x, y))
        continue;

      XY xy(x, y);
      if (mindistance > 0)
      {
        if (selected_coordinates.nearest(xy, mindistance))
          continue;
        selected_coordinates.insert(xy);
      }

      accepted.push_back({x, y, location->name, location->feature, location->population});
    }

    // ----------------------------------------------------------------
    // PASS 2 (optional): run label placement on the accepted set.
    // Text dimensions are measured here so that the pure placement
    // algorithms in LabelPlacement.cpp have no TextUtility dependency.
    // ----------------------------------------------------------------

    std::vector<PlacedLabel> placed_labels;

    if (!label_config.empty())
    {
      std::vector<LabelCandidate> label_candidates;
      label_candidates.reserve(accepted.size());

      for (const auto& al : accepted)
      {
        LabelCandidate cand;
        cand.anchor_x = al.x;
        cand.anchor_y = al.y;
        cand.text = al.name;
        cand.population = static_cast<double>(al.population);
        cand.font_size = label_config.effective_font_size(cand.population);
        cand.font_weight = label_config.effective_font_weight(cand.population);
        cand.fill = label_config.effective_fill(cand.population);

        text_style_t style;
        style.fontfamily = label_config.font_family;
        style.fontsize = Fmi::to_string(lround(cand.font_size));
        style.fontweight = cand.font_weight;
        const auto dim = getTextDimension(al.name, style);
        cand.label_w = dim.width;
        cand.label_h = dim.height;

        label_candidates.push_back(cand);
      }

      placed_labels = placeLabels(label_config,
                                  std::move(label_candidates),
                                  static_cast<double>(box.width()),
                                  static_cast<double>(box.height()));
    }

    // ----------------------------------------------------------------
    // PASS 3: emit SVG.
    // Symbols first, then labels — labels render on top within the
    // group.  Across layers, weather data should sit above the location
    // layer (higher z-order in the product JSON).
    // ----------------------------------------------------------------

    // The <g> wrapper holds symbols as nested tags. Text labels cannot
    // ride along as nested tags because the SVG template does not render
    // tag.cdata — they must be emitted as separate top-level layers, and
    // the </g> close is concatenated onto the last one. Same pattern as
    // NumberLayer::generate_qEngine.
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Add layer attributes to the group, not to the individual elements
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Emit symbols (as nested tags inside the group, since <use ... /> is
    // self-closing and works with the template's nested-tag rendering)
    for (const auto& al : accepted)
    {
      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      std::string iri;

      if (symbol)
        iri = *symbol;

      if (!symbols.empty())
      {
        auto feature_selection = symbols.find(al.feature);
        if (feature_selection == symbols.end())
          feature_selection = symbols.find("default");
        if (feature_selection != symbols.end())
        {
          auto selection =
              Select::attribute(feature_selection->second,
                                static_cast<double>(al.population));
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
        tag_cdt["attributes"]["x"] = Fmi::to_string(lround(al.x));
        tag_cdt["attributes"]["y"] = Fmi::to_string(lround(al.y));

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    theLayersCdt.PushBack(group_cdt);

    // Emit labels (after symbols so text renders above symbols within
    // the same SVG group)
    for (const auto& pl : placed_labels)
    {
      if (!pl.placed)
        continue;

      // Bounding box top-left is (pl.bbox.x1, pl.bbox.y1).
      // SVG text y is the baseline; we set it to the bottom of the
      // bounding box (conservative: all measured height treated as
      // above-baseline).
      const double text_x = pl.bbox.x1;
      const double text_y = pl.bbox.y2;

      emitText(theLayersCdt, pl, label_config, text_x, text_y);
    }

    // Close the <g> wrapper. Always concatenated onto the last pushed
    // layer's end — that is group_cdt itself if no labels were placed,
    // or the last text element otherwise.
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

// ======================================================================
// LocationLayer::getFeatureInfo
// ======================================================================

void LocationLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
  // no info available
}

// ======================================================================
// LocationLayer::hash_value
// ======================================================================

std::size_t LocationLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(keyword));
    Fmi::hash_combine(hash, Fmi::hash_value(mindistance));
    Fmi::hash_combine(hash, Fmi::hash_value(countries));
    Fmi::hash_combine(hash, Fmi::hash_value(symbol));
    Fmi::hash_combine(hash, Dali::hash_symbol(symbol, theState));

    for (const auto& name_symbol : symbols)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(name_symbol.first));
      for (const auto& selection : name_symbol.second)
        Fmi::hash_combine(hash, Dali::hash_value(selection, theState));
    }

    Fmi::hash_combine(hash, hashLabelConfig(label_config));

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
