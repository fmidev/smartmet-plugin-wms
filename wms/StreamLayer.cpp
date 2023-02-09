//======================================================================

#include "StreamLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoline.h"
#include "Layer.h"
#include "State.h"
#include "StyleSheet.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/GraphFunctions.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <trax/InterpolationType.h>

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

void StreamLayer::init(const Json::Value& theJson,
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

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("interpolation", nulljson);
    if (!json.isNull())
      interpolation = json.asString();

    json = theJson.get("maxStreamLength", nulljson);
    if (!json.isNull())
      maxStreamLen = json.asInt();

    json = theJson.get("minStreamLength", nulljson);
    if (!json.isNull())
      minStreamLen = json.asInt();

    json = theJson.get("lineLength", nulljson);
    if (!json.isNull())
      lineLen = json.asInt();

    json = theJson.get("xStep", nulljson);
    if (!json.isNull())
      xStep = json.asInt();

    json = theJson.get("yStep", nulljson);
    if (!json.isNull())
      yStep = json.asInt();

    json = theJson.get("precision", nulljson);
    if (!json.isNull())
      precision = json.asDouble();

    json = theJson.get("minarea", nulljson);
    if (!json.isNull())
      minarea = json.asDouble();

    json = theJson.get("unit_conversion", nulljson);
    if (!json.isNull())
      unit_conversion = json.asString();

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();

    json = theJson.get("outside", nulljson);
    if (!json.isNull())
    {
      outside.reset(Map());
      outside->init(json, theConfig);
    }

    json = theJson.get("inside", nulljson);
    if (!json.isNull())
    {
      inside.reset(Map());
      inside->init(json, theConfig);
    }

    json = theJson.get("sampling", nulljson);
    if (!json.isNull())
      sampling.init(json, theConfig);

    json = theJson.get("intersect", nulljson);
    if (!json.isNull())
      intersections.init(json, theConfig);
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

    if (!parameter)
      throw Fmi::Exception(BCP, "Parameter not set for stream-layer");

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);

    // Alter units if requested
    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    std::string wkt = *projection.crs;
    // std::cout << wkt << "\n";

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

      char bbox[100];

      const auto& box = projection.getBox();
      const auto clipbox = getClipBox(box);

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      sprintf(bbox, "%f,%f,%f,%f", bl.X(), bl.Y(), tr.X(), tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      sprintf(bbox, "%f,%f,%f,%f", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(), clipbox.ymax());
      // sprintf(bbox, "%f,%f,%f,%f", box.xmin(), box.ymin(), box.xmax(), box.ymax());
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means
      // that we we do not know the actual projection yet and we have to wait that the grid-engine
      // delivers us the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    std::string pName = *parameter;
    auto pos = pName.find(".raw");
    if (pos != std::string::npos)
    {
      attributeList.addAttribute("areaInterpolationMethod",std::to_string(T::AreaInterpolationMethod::Nearest));
      pName.erase(pos, 4);
    }

    std::string param = gridEngine->getParameterString(producerName, pName);
    attributeList.addAttribute("param", param);

    if (!projection.projectionParameter)
      projection.projectionParameter = param;

    if (param == *parameter && originalGridQuery->mProducerNameList.empty())
    {
      gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
      if (originalGridQuery->mProducerNameList.empty())
        originalGridQuery->mProducerNameList.push_back(producerName);
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
      param.mType = QueryServer::QueryParameter::Type::StreamLine;

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
      char bbox[100];
      sprintf(bbox, "%f,%f,%f,%f", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }

    originalGridQuery->mAttributeList.setAttribute("stream.coordinateType",std::to_string(static_cast<int>(T::CoordinateTypeValue::ORIGINAL_COORDINATES)));
    originalGridQuery->mAttributeList.setAttribute("stream.minLength",std::to_string(minStreamLen));
    originalGridQuery->mAttributeList.setAttribute("stream.maxLength",std::to_string(maxStreamLen));
    originalGridQuery->mAttributeList.setAttribute("stream.lineLength",std::to_string(lineLen));
    originalGridQuery->mAttributeList.setAttribute("stream.xStep",std::to_string(xStep));
    originalGridQuery->mAttributeList.setAttribute("stream.yStep",std::to_string(yStep));

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

    // Converting the returned WKB-streamlines into OGRGeometry objects.

    std::vector<OGRGeometryPtr> geoms;
    for (const auto& param : query->mQueryParameterList)
    {
      for (const auto& val : param.mValueList)
      {
        if (!val->mValueData.empty())
        {
          for (const auto& wkb : val->mValueData)
          {
            const auto* cwkb = reinterpret_cast<const unsigned char*>(wkb.data());
            OGRGeometry* geom = nullptr;
            OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb.size());
            auto geomPtr = OGRGeometryPtr(geom);
            geoms.push_back(geomPtr);
          }
        }
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
 * \brief Generate the isoline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> StreamLayer::getStreamsQuerydata(const State& theState)
{
  std::vector<OGRGeometryPtr> geoms;

#if 0 // Under construction

  // Establish the data. Store to member variable for IsolabelLayer use

  auto qEngine = theState.getQEngine();

  q = getModel(theState);

  if (q && !(q->isGrid()))
    throw Fmi::Exception(BCP, "Stream-layer can't use point data!");

  // Establish the desired direction parameter

  if (!parameter)
    throw Fmi::Exception(BCP, "Parameter not set for stream-layer");

  auto param = TS::ParameterFactory::instance().parse(*parameter);

  // Establish the valid time

  auto valid_time = getValidTime();

  // Establish the level

  if (q && !q->firstLevel())
    throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

  if (level)
  {
    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate streamlines without gridded level data");

    if (!q->selectLevel(*level))
      throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available!");
  }

  // Get projection details

  projection.update(q);
  const auto& crs = projection.getCRS();
  const auto& box = projection.getBox();
  const auto clipbox = getClipBox(box);

  // Sample to higher resolution if necessary

  auto sampleresolution = sampling.getResolution(projection);
  if (sampleresolution)
  {
    if (!q)
      throw Fmi::Exception(BCP, "Cannot resample without gridded data");

    auto demdata = theState.getGeoEngine().dem();
    auto landdata = theState.getGeoEngine().landCover();
    if (!demdata || !landdata)
      throw Fmi::Exception(
          BCP, "Resampling data in StreamLayer requires DEM and land cover data to be available");

    q = q->sample(param,
                  valid_time,
                  crs,
                  box.xmin(),
                  box.ymin(),
                  box.xmax(),
                  box.ymax(),
                  *sampleresolution,
                  *demdata,
                  *landdata);
  }

  if (!q)
    throw Fmi::Exception(BCP, "Cannot generate streamlines without gridded data");

  // Calculate the streamlines

  T::ParamValue_vec gridValues;
  std::vector<T::Coordinate> coordinates;
  T::ByteData_vec streamlines;

  auto cm = q->CoordinateMatrix();
  int width = cm.width();
  int height = cm.height();
  int sz = width * height;


  coordinates.reserve(sz);
  gridValues.reserve(sz);


  boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
  boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));

  auto valid_time_period = getValidTimePeriod();
  NFmiMetTime met_time = valid_time_period.begin();
  boost::local_time::local_date_time localdatetime(met_time, utc);
  std::string tmp;
  auto mylocale = std::locale::classic();
  NFmiPoint dummy;
  TimeSeries::LocalTimePoolPtr localTimePool = nullptr;

  for (int y=0; y<height; y++)
  {
    for (int x=0; x<width; x++)
    {
      double xx = cm.x(x,y);
      double yy = cm.y(x,y);

      coordinates.push_back(T::Coordinate(xx,yy));

      Spine::Location loc(x,y);

      auto p = Engine::Querydata::ParameterOptions(param,tmp,loc,tmp,tmp,
                 *timeformatter,tmp,tmp,mylocale,tmp,false,dummy,dummy,localTimePool);

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
    }
  }

  getStreamlines(gridValues,&coordinates,width,height,minStreamLen,maxStreamLen,lineLen,xStep,yStep,streamlines);

  for (const auto& wkb : streamlines)
  {
    const auto* cwkb = reinterpret_cast<const unsigned char*>(wkb.data());
    OGRGeometry* geom = nullptr;
    OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb.size());
    auto geomPtr = OGRGeometryPtr(geom);
    geoms.push_back(geomPtr);
  }
#endif

  return geoms;
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

    std::string report = "StreamLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

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
    std::string objectKey = "streamline:" + *parameter + ":" + qid;
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
        std::string iri = qid + (qid.empty() ? "" : ".") + std::to_string(i); // isoline.getQid(theState);

        if (!theState.addId(iri))
          throw Fmi::Exception(BCP, "Non-unique ID assigned to streamline").addParameter("ID", iri);

        std::string arcNumbers;
        std::string arcCoordinates;
        //precision = 5.0;
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
    Fmi::hash_combine(hash, Fmi::hash_value(interpolation));
    Fmi::hash_combine(hash, Fmi::hash_value(extrapolation));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Fmi::hash_value(minarea));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Dali::hash_value(outside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(inside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(sampling, theState));
    Fmi::hash_combine(hash, Dali::hash_value(intersections, theState));
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
