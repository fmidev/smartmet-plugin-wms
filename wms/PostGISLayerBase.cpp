#include "PostGISLayerBase.h"
#include "Hash.h"

#include <spine/Exception.h>
#include <spine/Json.h>
#include <engines/gis/Engine.h>

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
      throw SmartMet::Spine::Exception(BCP, "PostGIS JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    Json::Value nulljson;

    auto json = theJson.get("pgname", nulljson);
    if (!json.isNull())
      this->pgname = json.asString();
    else
      throw SmartMet::Spine::Exception(BCP, "'pgname' must be defined for postgis layer");

    json = theJson.get("schema", nulljson);
    if (!json.isNull())
      this->schema = json.asString();
    else
      throw SmartMet::Spine::Exception(BCP, "'schema' must be defined for postgis layer");

    json = theJson.get("table", nulljson);
    if (!json.isNull())
      this->table = json.asString();
    else
      throw SmartMet::Spine::Exception(BCP, "'table' must be defined for postgis layer");

    json = theJson.get("time_column", nulljson);
    if (!json.isNull())
      time_column = json.asString();

    // If following does not pass, time_condition will be empty
    if (time_column && time)
    {
      std::string timestr = "'" + boost::posix_time::to_iso_string(*time) + "'";
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
          throw SmartMet::Spine::Exception(
              BCP, "Invalid value for time truncation: '" + time_truncate + "'");
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
      SmartMet::Spine::JSON::extract_array("filters", this->filters, json, theConfig);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// Hash value
std::size_t PostGISLayerBase::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, Dali::hash_value(pgname));
    boost::hash_combine(hash, Dali::hash_value(schema));
    boost::hash_combine(hash, Dali::hash_value(table));
    boost::hash_combine(hash, Dali::hash_value(time_column));
    boost::hash_combine(hash, Dali::hash_value(time_condition));
    boost::hash_combine(hash, Dali::hash_value(filters, theState));
    return hash;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get shape from database
 */
// ----------------------------------------------------------------------

OGRGeometryPtr PostGISLayerBase::getShape(const State& theState,
                                          OGRSpatialReference* theSR,
                                          SmartMet::Engine::Gis::MapOptions& theMapOptions) const
{
  try
  {
    OGRGeometryPtr geom;
    const auto& gis = theState.getGisEngine();
    {
      geom = gis.getShape(theSR, theMapOptions);

      if (!geom && !geom->IsEmpty())
      {
        std::string msg = "Requested map data is empty: '" + theMapOptions.schema + '.' +
                          theMapOptions.table + "'";
        if (theMapOptions.minarea)
          msg += " Is the minarea limit too large?";

        throw SmartMet::Spine::Exception(BCP, msg);
      }
    }

    return geom;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get shapes with associated attributes from database
 */
// ----------------------------------------------------------------------

Fmi::Features PostGISLayerBase::getFeatures(const State& theState,
                                            OGRSpatialReference* theSR,
                                            SmartMet::Engine::Gis::MapOptions& theMapOptions) const
{
  try
  {
    const auto& gis = theState.getGisEngine();

    Fmi::Features result_set = gis.getFeatures(theSR, theMapOptions);

    for (auto result_item : result_set)
    {
      if (!result_item->geom || result_item->geom->IsEmpty())
      {
        std::string msg = "Requested map data is empty: '" + theMapOptions.schema + '.' +
                          theMapOptions.table + "'";
        if (theMapOptions.minarea)
          msg += " Is the minarea limit too large?";

        throw SmartMet::Spine::Exception(BCP, msg);
      }
    }

    return result_set;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
