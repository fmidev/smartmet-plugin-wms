#include "PostGISLayerBase.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialization
 */
// ----------------------------------------------------------------------

void PostGISLayerBase::init(Json::Value& theJson,
                            const State& theState,
                            const Config& theConfig,
                            const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "PostGIS JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("icemap");

    JsonTools::remove_string(pgname, theJson, "pgname");
    JsonTools::remove_string(schema, theJson, "schema");
    JsonTools::remove_string(table, theJson, "table");

    if (pgname.empty())
      throw Fmi::Exception(BCP, "'pgname' must be defined for postgis layer");
    if (schema.empty())
      throw Fmi::Exception(BCP, "'schema' must be defined for postgis layer");
    if (table.empty())
      throw Fmi::Exception(BCP, "'table' must be defined for postgis layer");

    JsonTools::remove_double(precision, theJson, "precision");
    JsonTools::remove_string(time_column, theJson, "time_column");
    JsonTools::remove_bool(lines, theJson, "lines");

    // If following does not pass, time_condition will be empty
    if (time_column && hasValidTime())
    {
      std::string timestr = "'" + Fmi::to_iso_string(getValidTime()) + "'";
      // time truncate is optional
      std::string trunc;
      JsonTools::remove_string(trunc, theJson, "time_truncate");
      // We do not support microseconds, milliseconds, seconds nor decades etc supported by
      // PostGreSQL

      if (!trunc.empty())
      {
        if (trunc != "minute" && trunc != "hour" && trunc != "day" && trunc != "week" &&
            trunc != "month" && trunc != "year")
        {
          throw Fmi::Exception(BCP, "Invalid value for time truncation: '" + trunc + "'");
        }

        time_condition = fmt::format(
            "DATE_TRUNC('{}',{}) = DATE_TRUNC('{}',date {})", trunc, *time_column, trunc, timestr);
      }
      else
      {
        // No truncate
        time_condition = *time_column + "=" + timestr;
      }
    }

    // filter for query, each filter generates one PostGIS query
    auto json = JsonTools::remove(theJson, "filters");
    if (!json.isNull())
      JsonTools::extract_array("filters", this->filters, json, theConfig);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Hash value
std::size_t PostGISLayerBase::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(pgname));
    Fmi::hash_combine(hash, Fmi::hash_value(schema));
    Fmi::hash_combine(hash, Fmi::hash_value(table));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Fmi::hash_value(time_column));
    Fmi::hash_combine(hash, Fmi::hash_value(time_condition));
    Fmi::hash_combine(hash, Dali::hash_value(filters, theState));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get shape from database
 */
// ----------------------------------------------------------------------

OGRGeometryPtr PostGISLayerBase::getShape(const State& theState,
                                          const Fmi::SpatialReference& theSR,
                                          Engine::Gis::MapOptions& theMapOptions)
{
  try
  {
    OGRGeometryPtr geom;
    const auto& gis = theState.getGisEngine();
    {
      geom = gis.getShape(&theSR, theMapOptions);

      if (!geom)
      {
        std::string msg = "Requested map data is empty: '" + theMapOptions.schema + '.' +
                          theMapOptions.table + "'";
        if (theMapOptions.minarea)
          msg += " Is the minarea limit too large?";

        throw Fmi::Exception(BCP, msg);
      }
    }

    return geom;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get shapes with associated attributes from database
 */
// ----------------------------------------------------------------------

Fmi::Features PostGISLayerBase::getFeatures(const State& theState,
                                            const Fmi::SpatialReference& theSR,
                                            Engine::Gis::MapOptions& theMapOptions)
{
  try
  {
    const auto& gis = theState.getGisEngine();

    Fmi::Features result_set = gis.getFeatures(theSR, theMapOptions);

    for (const auto& result_item : result_set)
    {
      if (!result_item->geom || result_item->geom->IsEmpty() != 0)
      {
        std::string msg = "Requested map data is empty: '" + theMapOptions.schema + '.' +
                          theMapOptions.table + "'";
        if (theMapOptions.minarea)
          msg += " Is the minarea limit too large?";

        throw Fmi::Exception(BCP, msg);
      }
    }

    return result_set;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
