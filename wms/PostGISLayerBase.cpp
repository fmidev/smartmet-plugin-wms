#include "PostGISLayerBase.h"
#include "Config.h"
#include "Hash.h"
#include <engines/gis/Engine.h>
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

void PostGISLayerBase::init(const Json::Value& theJson,
                            const State& theState,
                            const Config& theConfig,
                            const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "PostGIS JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    this->precision = theConfig.defaultPrecision("icemap");

    Json::Value nulljson;

    auto json = theJson.get("pgname", nulljson);
    if (!json.isNull())
      this->pgname = json.asString();
    else
      throw Fmi::Exception(BCP, "'pgname' must be defined for postgis layer");

    json = theJson.get("schema", nulljson);
    if (!json.isNull())
      this->schema = json.asString();
    else
      throw Fmi::Exception(BCP, "'schema' must be defined for postgis layer");

    json = theJson.get("table", nulljson);
    if (!json.isNull())
      this->table = json.asString();
    else
      throw Fmi::Exception(BCP, "'table' must be defined for postgis layer");

    json = theJson.get("precision", nulljson);
    if (!json.isNull())
      this->precision = json.asDouble();

    json = theJson.get("time_column", nulljson);
    if (!json.isNull())
      time_column = json.asString();

    json = theJson.get("lines", nulljson);
    if (!json.isNull())
      lines = json.asBool();

    // If following does not pass, time_condition will be empty
    if (time_column && time)
    {
      std::string timestr = "'" + Fmi::to_iso_string(*time) + "'";
      // time truncate is optional
      json = theJson.get("time_truncate", nulljson);
      if (!json.isNull())
      {
        std::string time_truncate = json.asString();
        // We do not support microseconds, milliseconds, seconds nor decades etc supported by
        // PostGreSQL
        if (time_truncate != "minute" && time_truncate != "hour" && time_truncate != "day" &&
            time_truncate != "week" && time_truncate != "month" && time_truncate != "year")
        {
          throw Fmi::Exception(BCP, "Invalid value for time truncation: '" + time_truncate + "'");
        }

        std::string trunc = "DATE_TRUNC('" + time_truncate + "',";
        time_condition = trunc + *time_column + ") = " + trunc + "date " + timestr + ")";
      }
      else
      {
        // No truncate
        time_condition = *time_column + "=" + timestr;
      }
    }

    // filter for query, each filter generates one PostGIS query
    json = theJson.get("filters", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("filters", this->filters, json, theConfig);
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
    Dali::hash_combine(hash, Dali::hash_value(pgname));
    Dali::hash_combine(hash, Dali::hash_value(schema));
    Dali::hash_combine(hash, Dali::hash_value(table));
    Dali::hash_combine(hash, Dali::hash_value(precision));
    Dali::hash_combine(hash, Dali::hash_value(time_column));
    Dali::hash_combine(hash, Dali::hash_value(time_condition));
    Dali::hash_combine(hash, Dali::hash_value(filters, theState));
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
                                          Engine::Gis::MapOptions& theMapOptions) const
{
  try
  {
    OGRGeometryPtr geom;
    const auto& gis = theState.getGisEngine();
    {
      geom = gis.getShape(&theSR, theMapOptions);

      if (!geom && geom->IsEmpty() == 0)
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
                                            Engine::Gis::MapOptions& theMapOptions) const
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
