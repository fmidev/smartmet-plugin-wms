//======================================================================

#include "StreamLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoline.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include "StyleSheet.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/GraphFunctions.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <trax/InterpolationType.h>

const double pi = boost::math::constants::pi<double>();

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

void StreamLayer::init(Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Strea,-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("stream");

    // Extract member values

    JsonTools::remove_string(parameter, theJson, "parameter");
    JsonTools::remove_string(u_parameter, theJson, "u");
    JsonTools::remove_string(v_parameter, theJson, "v");
    JsonTools::remove_int(maxStreamLen, theJson, "max_length");
    JsonTools::remove_int(minStreamLen, theJson, "min_length");
    JsonTools::remove_int(lineLen, theJson, "line_length");
    JsonTools::remove_int(xStep, theJson, "xstep");
    JsonTools::remove_int(yStep, theJson, "ystep");
    JsonTools::remove_double(precision, theJson, "precision");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the streamline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> StreamLayer::getStreams(State& theState)
{
  try
  {
    std::vector<OGRGeometryPtr> geoms;
    if (source && *source == "grid")
      geoms = getStreamsGrid(theState);
    else
      geoms = getStreamsQuerydata(theState);

    return geoms;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the streamline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> StreamLayer::getStreamsGrid(State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    // if (!parameter)
    //  throw Fmi::Exception(BCP, "Parameter not set for stream-layer");

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);
    std::string wkt = *projection.crs;

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
      {
        auto crs = projection.getCRS();
        char* out = nullptr;
        crs.get()->exportToWkt(&out);
        wkt = out;
        CPLFree(out);
      }

      // Adding the bounding box information into the query.

      const auto& box = projection.getBox();
      const auto clipbox = getClipBox(box);

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      std::string bbox = fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      bbox = fmt::format(
          "{},{},{},{}", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(), clipbox.ymax());
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means
      // that we we do not know the actual projection yet and we have to wait that the grid-engine
      // delivers us the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    if (parameter)
    {
      std::string param = gridEngine->getParameterString(producerName, *parameter);
      attributeList.addAttribute("param", param);

      if (!projection.projectionParameter)
        projection.projectionParameter = param;

      if (param == *parameter && originalGridQuery->mProducerNameList.empty())
      {
        gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
        if (originalGridQuery->mProducerNameList.empty())
          originalGridQuery->mProducerNameList.push_back(producerName);
      }
    }
    else if (u_parameter && v_parameter)
    {
      std::string u_param = gridEngine->getParameterString(producerName, *u_parameter);
      std::string v_param = gridEngine->getParameterString(producerName, *v_parameter);

      attributeList.addAttribute("param", u_param + "," + v_param);

      if (!projection.projectionParameter)
        projection.projectionParameter = u_param;

      if (u_param == *u_parameter && originalGridQuery->mProducerNameList.empty())
      {
        gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
        if (originalGridQuery->mProducerNameList.empty())
          originalGridQuery->mProducerNameList.push_back(producerName);
      }
    }
    else
    {
      throw Fmi::Exception(BCP,
                           "Missing 'parameter' (= direction) or 'u' / 'v' vector definitions");
    }

    std::string forecastTime = Fmi::to_iso_string(getValidTime());
    attributeList.addAttribute("startTime", forecastTime);
    attributeList.addAttribute("endTime", forecastTime);
    attributeList.addAttribute("timelist", forecastTime);
    attributeList.addAttribute("timezone", "UTC");

    if (origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery, attributeList);

    // Fullfilling information into the query object.

    for (auto& param : originalGridQuery->mQueryParameterList)
    {
      param.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      param.mType = QueryServer::QueryParameter::Type::Vector;
      param.mFlags = QueryServer::QueryParameter::Flags::ReturnCoordinates;

      if (geometryId)
        param.mGeometryId = *geometryId;

      if (levelId)
        param.mParameterLevelId = *levelId;

      if (level)
        param.mParameterLevel = C_INT(*level);

      if (forecastType)
        param.mForecastType = C_INT(*forecastType);

      if (forecastNumber)
        param.mForecastNumber = C_INT(*forecastNumber);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    if (projection.size && *projection.size > 0)
    {
      originalGridQuery->mAttributeList.addAttribute("grid.size", std::to_string(*projection.size));
    }
    else
    {
      if (projection.xsize)
        originalGridQuery->mAttributeList.addAttribute("grid.width",
                                                       std::to_string(*projection.xsize));

      if (projection.ysize)
        originalGridQuery->mAttributeList.addAttribute("grid.height",
                                                       std::to_string(*projection.ysize));
    }

    if (wkt == "data" && projection.x1 && projection.y1 && projection.x2 && projection.y2)
    {
      std::string bbox = fmt::format(
          "{},{},{},{}", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }

    originalGridQuery->mAttributeList.setAttribute(
        "grid.coordinateType",
        std::to_string(static_cast<int>(T::CoordinateTypeValue::LATLON_COORDINATES)));

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // Extracting the projection information from the query result.

    if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
    {
      const char* widthStr = query->mAttributeList.getAttributeValue("grid.width");
      const char* heightStr = query->mAttributeList.getAttributeValue("grid.height");

      if (widthStr != nullptr)
        projection.xsize = Fmi::stoi(widthStr);

      if (heightStr != nullptr)
        projection.ysize = Fmi::stoi(heightStr);
    }

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    if (*projection.crs == "data")
    {
      const char* crsStr = query->mAttributeList.getAttributeValue("grid.crs");
      const char* proj4Str = query->mAttributeList.getAttributeValue("grid.proj4");
      if (proj4Str != nullptr && strstr(proj4Str, "+lon_wrap") != nullptr)
        crsStr = proj4Str;

      if (crsStr != nullptr)
      {
        projection.crs = crsStr;
        if (!projection.bboxcrs)
        {
          const char* bboxStr = query->mAttributeList.getAttributeValue("grid.bbox");
          if (bboxStr != nullptr)
          {
            std::vector<double> partList;
            splitString(bboxStr, ',', partList);

            if (partList.size() == 4)
            {
              projection.x1 = partList[0];
              projection.y1 = partList[1];
              projection.x2 = partList[2];
              projection.y2 = partList[3];
            }
          }
        }
      }
    }

    std::vector<OGRGeometryPtr> geoms;

    T::ByteData_vec streamlines;
    int gwidth = *projection.xsize;
    int gheight = *projection.ysize;
    std::size_t sz = gwidth * gheight;

    if (query->mQueryParameterList.size() == 1 &&
        query->mQueryParameterList[0].mValueList.size() == 1 &&
        query->mQueryParameterList[0].mValueList[0]->mValueVector.size() == sz)
    {
      // Grid contains directions (degrees)
      getStreamlines(query->mQueryParameterList[0].mValueList[0]->mValueVector,
                     &query->mQueryParameterList[0].mCoordinates,
                     gwidth,
                     gheight,
                     minStreamLen,
                     maxStreamLen,
                     lineLen,
                     xStep,
                     yStep,
                     streamlines);
    }
    else if (query->mQueryParameterList.size() == 2 &&
             query->mQueryParameterList[0].mValueList.size() == 1 &&
             query->mQueryParameterList[1].mValueList.size() == 1 &&
             query->mQueryParameterList[0].mCoordinates.size() == sz)
    {
      // Grid contains U- and V- parameters. Counting directions (degrees)
      T::ParamValue_vec gridValues;
      gridValues.reserve(sz);

      uint sz1 = query->mQueryParameterList[0].mValueList[0]->mValueVector.size();
      uint sz2 = query->mQueryParameterList[1].mValueList[0]->mValueVector.size();

      if (sz1 == sz && sz2 == sz)
      {
        for (uint t = 0; t < sz; t++)
        {
          double uspd = query->mQueryParameterList[0].mValueList[0]->mValueVector[t];
          double vspd = query->mQueryParameterList[1].mValueList[0]->mValueVector[t];

          if (uspd != ParamValueMissing && vspd != ParamValueMissing)
          {
            // double wspd = sqrt(uspd * uspd + vspd * vspd);
            if (uspd != 0 || vspd != 0)
            {
              double wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
              gridValues.push_back(wdir);
            }
            else
            {
              gridValues.push_back(ParamValueMissing);
            }
          }
          else
          {
            gridValues.push_back(ParamValueMissing);
          }
        }
        getStreamlines(gridValues,
                       &query->mQueryParameterList[0].mCoordinates,
                       gwidth,
                       gheight,
                       minStreamLen,
                       maxStreamLen,
                       lineLen,
                       xStep,
                       yStep,
                       streamlines);
      }
    }

    // Converting WKB lines to OGRGeometries:

    for (const auto& wkb : streamlines)
    {
      const auto* cwkb = reinterpret_cast<const unsigned char*>(wkb.data());
      OGRGeometry* geom = nullptr;
      OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb.size());
      auto geomPtr = OGRGeometryPtr(geom);
      geoms.push_back(geomPtr);
    }
    return geoms;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the isoline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> StreamLayer::getStreamsQuerydata(const State& theState)
{
  try
  {
    std::vector<OGRGeometryPtr> geoms;

    q = getModel(theState);

    if (q && !(q->isGrid()))
      throw Fmi::Exception(BCP, "Stream-layer can't use point data!");

    if (q && !q->firstLevel())
      throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

    if (level)
    {
      if (!q)
        throw Fmi::Exception(BCP, "Cannot generate streamlines without gridded level data");

      if (!q->selectLevel(*level))
        throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available!");
    }

    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate streamlines without gridded data");

    T::AttributeList attributeList;
    T::Coordinate_svec latlonCoordinates;
    uint gwidth;
    uint gheight;

    std::string wkt = *projection.crs;
    if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
    {
      auto crs = projection.getCRS();
      char* out = nullptr;
      crs.get()->exportToWkt(&out);
      wkt = out;
      CPLFree(out);
    }

    // Adding the bounding box information into the query.

    const auto& box = projection.getBox();
    const auto clipbox = getClipBox(box);

    auto bl = projection.bottomLeftLatLon();
    auto tr = projection.topRightLatLon();

    std::string bbox = fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y());
    attributeList.addAttribute("grid.llbox", bbox);

    bbox =
        fmt::format("{},{},{},{}", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(), clipbox.ymax());
    attributeList.addAttribute("grid.bbox", bbox);

    attributeList.addAttribute("grid.crs", wkt);

    if (projection.xsize)
      attributeList.addAttribute("grid.width", std::to_string(*projection.xsize));

    if (projection.ysize)
      attributeList.addAttribute("grid.height", std::to_string(*projection.ysize));

    // Getting coordinates for the new grid.
    Identification::gridDef.getGridLatLonCoordinatesByGeometry(
        attributeList, latlonCoordinates, gwidth, gheight);

    T::ParamValue_vec gridValues;
    T::ByteData_vec streamlines;
    std::size_t sz = gwidth * gheight;
    gridValues.reserve(sz);

    boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::TimeZonePtr utc(new boost::local_time::posix_time_zone("UTC"));

    auto valid_time_period = getValidTimePeriod();
    NFmiMetTime met_time = valid_time_period.begin();
    Fmi::LocalDateTime localdatetime(met_time, utc);
    std::string tmp;
    auto mylocale = std::locale::classic();
    NFmiPoint dummy;
    TimeSeries::LocalTimePoolPtr localTimePool = nullptr;

    // Fetching values for the new grid.

    uint c = 0;

    if (parameter)
    {
      auto param = TS::ParameterFactory::instance().parse(*parameter);

      for (uint y = 0; y < gheight; y++)
      {
        for (uint x = 0; x < gwidth; x++)
        {
          Spine::Location loc((*latlonCoordinates)[c].x(), (*latlonCoordinates)[c].y());

          auto p = Engine::Querydata::ParameterOptions(param,
                                                       tmp,
                                                       loc,
                                                       tmp,
                                                       tmp,
                                                       *timeformatter,
                                                       tmp,
                                                       tmp,
                                                       mylocale,
                                                       tmp,
                                                       false,
                                                       dummy,
                                                       dummy,
                                                       localTimePool);

          auto res = q->value(p, localdatetime);
          if (boost::get<double>(&res) != nullptr)
          {
            auto direction = *boost::get<double>(&res);
            gridValues.push_back(direction);
          }
          else
          {
            gridValues.push_back(ParamValueMissing);
          }
          c++;
        }
      }
    }
    else if (u_parameter && v_parameter)
    {
      auto u_param = TS::ParameterFactory::instance().parse(*u_parameter);
      auto v_param = TS::ParameterFactory::instance().parse(*v_parameter);

      for (uint y = 0; y < gheight; y++)
      {
        for (uint x = 0; x < gwidth; x++)
        {
          Spine::Location loc((*latlonCoordinates)[c].x(), (*latlonCoordinates)[c].y());

          auto p_u = Engine::Querydata::ParameterOptions(u_param,
                                                         tmp,
                                                         loc,
                                                         tmp,
                                                         tmp,
                                                         *timeformatter,
                                                         tmp,
                                                         tmp,
                                                         mylocale,
                                                         tmp,
                                                         false,
                                                         dummy,
                                                         dummy,
                                                         localTimePool);

          auto p_v = Engine::Querydata::ParameterOptions(v_param,
                                                         tmp,
                                                         loc,
                                                         tmp,
                                                         tmp,
                                                         *timeformatter,
                                                         tmp,
                                                         tmp,
                                                         mylocale,
                                                         tmp,
                                                         false,
                                                         dummy,
                                                         dummy,
                                                         localTimePool);

          auto res_u = q->value(p_u, localdatetime);
          auto res_v = q->value(p_v, localdatetime);

          if (boost::get<double>(&res_u) != nullptr && boost::get<double>(&res_v) != nullptr)
          {
            auto uspd = *boost::get<double>(&res_u);
            auto vspd = *boost::get<double>(&res_v);

            if (uspd != kFloatMissing && vspd != kFloatMissing)
            {
              // double wspd = sqrt(uspd * uspd + vspd * vspd);
              if (uspd != 0 || vspd != 0)
              {
                // Note: qengine fixes orientation automatically
                double wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
                gridValues.push_back(wdir);
              }
              else
              {
                gridValues.push_back(ParamValueMissing);
              }
            }
            else
            {
              gridValues.push_back(ParamValueMissing);
            }
          }
          else
          {
            gridValues.push_back(ParamValueMissing);
          }
          c++;
        }
      }
    }
    else
    {
      throw Fmi::Exception(BCP,
                           "Missing 'parameter' (= direction) or 'u' / 'v' vector definitions");
    }

    if (gridValues.size() == sz)
    {
      // Calculate the streamlines
      getStreamlines(gridValues,
                     latlonCoordinates.get(),
                     gwidth,
                     gheight,
                     minStreamLen,
                     maxStreamLen,
                     lineLen,
                     xStep,
                     yStep,
                     streamlines);

      for (const auto& wkb : streamlines)
      {
        const auto* cwkb = reinterpret_cast<const unsigned char*>(wkb.data());
        OGRGeometry* geom = nullptr;
        OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb.size());
        auto geomPtr = OGRGeometryPtr(geom);
        geoms.push_back(geomPtr);
      }
    }
    return geoms;
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

void StreamLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "StreamLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    auto geoms = getStreams(theState);

    // The above call guarantees these have been resolved:
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Update the globals
    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    CTPP::CDT object_cdt;
    std::string objectKey;

    if (parameter)
      objectKey = "streamline:" + *parameter + ":" + qid;
    else if (u_parameter && v_parameter)
      objectKey = "streamline:" + *u_parameter + "_" + *v_parameter + ":" + qid;

    object_cdt["objectKey"] = objectKey;

    // Clip if necessary
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate streamlines as use tags statements inside <g>..</g>
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add attributes to the group, not the streamlines
    theState.addAttributes(theGlobals, group_cdt, attributes);

    std::ostringstream pathOut;
    std::ostringstream useOut;

    for (unsigned int i = 0; i < geoms.size(); i++)
    {
      OGRGeometryPtr geom = geoms[i];

      if (geom && geom->IsEmpty() == 0)
      {
        // Store the path with unique QID
        std::string iri =
            qid + (qid.empty() ? "" : ".") + std::to_string(i);  // isoline.getQid(theState);

        if (!theState.addId(iri))
          throw Fmi::Exception(BCP, "Non-unique ID assigned to streamline").addParameter("ID", iri);

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

        pathOut << "<path id=\"" << iri << "\" d=\"" << pointCoordinates << "\"/>\n";
        useOut << "<use xlink:href=\"#" << iri << "\"/>\n";
      }
    }

    theGlobals["includes"][qid] = pathOut.str();
    theGlobals["bbox"] = std::to_string(box.xmin()) + "," + std::to_string(box.ymin()) + "," +
                         std::to_string(box.xmax()) + "," + std::to_string(box.ymax());
    theGlobals["objects"][objectKey] = object_cdt;
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -(int)precision);

    // We created only this one layer
    group_cdt["cdata"] = useOut.str();
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void StreamLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  if (theState.isObservation(producer))
    return;
  if (parameter)
  {
    ParameterInfo info(*parameter);
    info.producer = producer;
    info.level = level;
    add(infos, info);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t StreamLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    if (!(source && *source == "grid"))
      Fmi::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

    // The hash value of the grid producer is already added
    // in the "Properties" class.

    Fmi::hash_combine(hash, Fmi::hash_value(parameter));
    Fmi::hash_combine(hash, Fmi::hash_value(minStreamLen));
    Fmi::hash_combine(hash, Fmi::hash_value(maxStreamLen));
    Fmi::hash_combine(hash, Fmi::hash_value(lineLen));
    Fmi::hash_combine(hash, Fmi::hash_value(xStep));
    Fmi::hash_combine(hash, Fmi::hash_value(yStep));
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
