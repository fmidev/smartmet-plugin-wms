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

  // Marker obstacle.  symbol_size is a shorthand alias for symmetric
  // markers: it just sets both width and height to the same value.
  // Explicit symbol_width/symbol_height parsed afterwards override the
  // shorthand, so a user can mix them (e.g. size=8, override width=10).
  if (json.isMember("symbol_size"))
  {
    double sym_size = 0.0;
    JsonTools::remove_double(sym_size, json, "symbol_size");
    cfg.symbol_width = sym_size;
    cfg.symbol_height = sym_size;
  }
  JsonTools::remove_double(cfg.symbol_width, json, "symbol_width");
  JsonTools::remove_double(cfg.symbol_height, json, "symbol_height");
  JsonTools::remove_double(cfg.symbol_margin, json, "symbol_margin");

  // Cross-platform stability for placement
  JsonTools::remove_double(cfg.bbox_padding, json, "bbox_padding");
  {
    int q = static_cast<int>(cfg.bbox_quantum);
    JsonTools::remove_int(q, json, "bbox_quantum");
    if (q < 0)
      throw Fmi::Exception(BCP, "LocationLayer 'label.bbox_quantum' must be >= 0");
    cfg.bbox_quantum = static_cast<unsigned int>(q);
  }

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
      // Per-class symbol obstacle override.  symbol_size is the alias
      // for symmetric markers; explicit width/height parsed afterwards
      // override it.  Mirrors the top-level LabelConfig parsing.
      if (cls_json.isMember("symbol_size"))
      {
        const double s = cls_json["symbol_size"].asDouble();
        cls.symbol_width = s;
        cls.symbol_height = s;
      }
      if (cls_json.isMember("symbol_width"))
        cls.symbol_width = cls_json["symbol_width"].asDouble();
      if (cls_json.isMember("symbol_height"))
        cls.symbol_height = cls_json["symbol_height"].asDouble();
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
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.symbol_width));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.symbol_height));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.symbol_margin));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.bbox_padding));
  Fmi::hash_combine(hash, Fmi::hash_value(cfg.bbox_quantum));
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
    Fmi::hash_combine(hash, Fmi::hash_value(cls.symbol_width));
    Fmi::hash_combine(hash, Fmi::hash_value(cls.symbol_height));
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

      // Marker is centered on the anchor with overflow=visible.  Symbol
      // size is resolved per-candidate so different population classes
      // can use different marker sizes (e.g. small dots for villages,
      // larger dots for cities).
      const unsigned int quantum = label_config.bbox_quantum;

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
        cand.y_bearing = dim.y_bearing;

        // Round dimensions up to the next multiple of quantum.  This
        // absorbs sub-pixel font-metric drift between platforms (Rocky10
        // vs RHEL9 fontconfig substitution) so the first overlap test
        // doesn't flip and cascade through the rest of the placement.
        if (quantum > 1)
        {
          cand.label_w = ((cand.label_w + quantum - 1) / quantum) * quantum;
          cand.label_h = ((cand.label_h + quantum - 1) / quantum) * quantum;
        }

        const double cand_sym_half_w =
            label_config.effective_symbol_width(cand.population) / 2.0 +
            label_config.symbol_margin;
        const double cand_sym_half_h =
            label_config.effective_symbol_height(cand.population) / 2.0 +
            label_config.symbol_margin;
        if (cand_sym_half_w > 0.0 && cand_sym_half_h > 0.0)
        {
          cand.obstacle = {al.x - cand_sym_half_w, al.y - cand_sym_half_h,
                           al.x + cand_sym_half_w, al.y + cand_sym_half_h};
        }

        label_candidates.push_back(cand);
      }

      placed_labels = placeLabels(label_config,
                                  std::move(label_candidates),
                                  static_cast<double>(box.width()),
                                  static_cast<double>(box.height()));
    }

    // ----------------------------------------------------------------
    // PASS 3: emit SVG.
    //
    // Render order within the group:
    //   1) labels (halo first, then fill)
    //   2) markers
    // Drawing markers last keeps them visually clean even when the label
    // halo would otherwise bleed onto them — useful when offset is small.
    //
    // Across layers in the product, weather data should sit above the
    // location layer (higher z-order in the product JSON).
    // ----------------------------------------------------------------

    // Open the <g> wrapper as a stand-alone layer (no nested tags).  All
    // labels and markers are pushed afterwards as top-level siblings so
    // the render order is exactly the push order.  The </g> close is
    // concatenated onto the last layer pushed — the group_cdt itself if
    // nothing follows, otherwise the last marker (or last label, if no
    // markers were emitted).
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Add layer attributes to the group, not to the individual elements
    theState.addAttributes(theGlobals, group_cdt, attributes);

    theLayersCdt.PushBack(group_cdt);

    // Emit labels first (so markers can render on top of them).
    // Iterate in reverse input order: input order is geonames priority
    // (highest first), so reversing means highest-priority text renders
    // last and stays visually on top when labels overlap.  Mostly only
    // matters for the `fixed` algorithm (no conflict detection); the
    // others avoid overlap by construction but the reverse order is a
    // safe default.
    for (size_t i = placed_labels.size(); i-- > 0;)
    {
      const auto& pl = placed_labels[i];
      if (!pl.placed)
        continue;

      // Position-aware baseline so the visible *letter body* touches
      // the marker on the side facing it (rather than the bbox, which
      // includes descender extent).  Descenders for above-labels then
      // hang into the empty area below the body without affecting how
      // the body's vertical position lines up across labels:
      //   - ABOVE the anchor (NE/NW/N/...): baseline = bbox.y2
      //     (body bottom at marker top edge; descender extends below)
      //   - BELOW the anchor (SE/SW/S/...): baseline = bbox.y1 + ascent
      //     (body top at marker bottom edge; descender extends below
      //     bbox into open space)
      //   - SIDE (E/W): body centred on the anchor (baseline = ay + ascent/2)
      // ascent is recovered from y_bearing (= -ascent in cairo).
      const double ay = accepted[i].y;
      const double y_bearing = static_cast<double>(pl.y_bearing);
      const double text_y = (pl.bbox.y2 <= ay)   ? pl.bbox.y2
                            : (pl.bbox.y1 >= ay) ? pl.bbox.y1 - y_bearing
                                                 : ay - y_bearing / 2.0;

      const double text_x = pl.bbox.x1;
      emitText(theLayersCdt, pl, label_config, text_x, text_y);
    }

    // Then emit markers.  They MUST go through layer.tags (not as
    // top-level layer entries) because the SVG template hard-codes a `>`
    // after layer attributes, which would mangle self-closing `<use ... />`
    // into `<use ...>\n/>`.  A single collector-layer with empty start
    // and end carries all <use> elements as nested tags so each renders
    // as one self-contained `<use attrs/>`.
    //
    // Iterated in reverse input order for the same reason as labels:
    // if two markers happen to land on top of each other (e.g. tiny
    // mindistance), the highest-priority one stays visible.
    // When label placement is active, suppress markers whose label was
    // dropped — an isolated marker on the map confuses the reader because
    // they cannot identify what location it points to.
    CTPP::CDT marker_cdt(CTPP::CDT::HASH_VAL);
    marker_cdt["start"] = "";
    marker_cdt["end"] = "";
    bool any_marker = false;

    const bool labels_active = !label_config.empty();
    for (size_t i = accepted.size(); i-- > 0;)
    {
      if (labels_active && i < placed_labels.size() && !placed_labels[i].placed)
        continue;

      const auto& al = accepted[i];

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

        marker_cdt["tags"].PushBack(tag_cdt);
        any_marker = true;
      }
    }

    if (any_marker)
      theLayersCdt.PushBack(marker_cdt);

    // Close the <g> wrapper on whichever layer was pushed last.  This is
    // the group_cdt itself if neither labels nor markers were emitted.
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
