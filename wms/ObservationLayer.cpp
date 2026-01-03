#ifndef WITHOUT_OBSERVATION

#include "ObservationLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "ObservationReader.h"
#include "PointData.h"
#include "ValueTools.h"
#include <ctpp2/CDT.hpp>
#include <engines/observation/Engine.h>
#include <gis/SpatialReference.h>
#include <timeseries/TimeSeriesInclude.h>

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

void ObservationLayer::init(Json::Value& theJson,
                            const State& theState,
                            const Config& theConfig,
                            const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Observation-layer JSON is not a JSON hash");

    Layer::init(theJson, theState, theConfig, theProperties);

    if (!producer)
      throw Fmi::Exception::Trace(BCP, "Producer must be defined for obervation layer!");
    if (!timestep)
      throw Fmi::Exception::Trace(BCP, "Timestep must be defined for obervation layer!");

    auto json = JsonTools::remove(theJson, "positions");
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }
    JsonTools::remove_double(maxdistance, theJson, "maxdistance");
    json = JsonTools::remove(theJson, "label");
    label.init(json, theConfig);

    // DEPRECATED:
    std::string keyword;
    JsonTools::remove_string(keyword, theJson, "keyword");
    if (!keyword.empty())
    {
      if (!positions)
      {
        positions = Positions{};
        positions->init(producer, projection, getValidTime(), theState);
      }
      positions->layout = Positions::Layout::Keyword;
      positions->keyword = keyword;
    }

    JsonTools::remove_uint(mindistance, theJson, "mindistance");
    JsonTools::remove_int(missing_symbol, theJson, "missing");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ObservationLayer init failed!");
  }
}

StationSymbolPriorities ObservationLayer::getProcessedData(State& theState) const
{
  try
  {
    auto valid_time = getValidTime();
    auto valid_time_period = getValidTimePeriod();

    auto parameters = getParameters();

    Fmi::SpatialReference crs = projection.getCRS();
    Fmi::Box box = projection.getBox();

    auto pointvalues = ObservationReader::read(theState,
                                               parameters,
                                               *this,
                                               *positions,
                                               maxdistance,
                                               crs,
                                               box,
                                               valid_time,
                                               valid_time_period);

    return processResultSet(pointvalues);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObservationLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (theState.getConfig().obsEngineDisabled())
      throw std::runtime_error(
          "Cannot use ObservationLayer when the observation engine is disabled");

    // Make sure positions are initialized
    if (!positions)
    {
      positions = Positions{};
      positions->layout = Positions::Layout::Data;
    }

    // Generate numbers as text layers inside <g>..</g>
    // Tags do not work, they do not have cdata enabled in the template

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Add attributes to the group, not the text tags
    theState.addAttributes(theGlobals, group_cdt, attributes);

    theLayersCdt.PushBack(group_cdt);

    // Get data from obsengine, do the aggregation, transform longitude/latitude to screen
    // coordinates, prioritize stations
    StationSymbolPriorities ssps = getProcessedData(theState);

    Fmi::NearTree<StationSymbolPriority> stations_to_show;

    for (const auto& ssp : ssps)
    {
      // Skip the station if it is too close to some station already on map
      if (mindistance > 0)
      {
        auto match = stations_to_show.nearest(ssp, mindistance);

        if (match)
          continue;

        stations_to_show.insert(ssp);
      }

      std::string chr;
      chr.append(1, ssp.symbol);
      if (chr.empty())
      {
        std::cout << "No symbol for station: " << ssp.fmisid << '\n';
        continue;
      }

      // Start generating the hash
      CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
      text_cdt["start"] = "<text";
      text_cdt["end"] = "</text>";
      text_cdt["cdata"] = ("&#" + Fmi::to_string(ssp.symbol) + ";");

      auto xpos = lround(ssp.x() + label.dx);
      auto ypos = lround(ssp.y() + label.dy);

      text_cdt["attributes"]["x"] = Fmi::to_string(xpos);
      text_cdt["attributes"]["y"] = Fmi::to_string(ypos);
      theLayersCdt.PushBack(text_cdt);
    }

    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Generating template hash for observations failed!")
        .addParameter("qid", qid);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t ObservationLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    Fmi::hash_combine(hash, Dali::hash_value(positions, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(maxdistance));
    Fmi::hash_combine(hash, Dali::hash_value(label, theState));

    Fmi::hash_combine(hash, Fmi::hash_value(mindistance));
    Fmi::hash_combine(hash, Fmi::hash_value(missing_symbol));

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

#endif  // WITHOUT_OBSERVATION
