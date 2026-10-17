//======================================================================

#include "StreamLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "GridQueryTools.h"
#include "Hash.h"
#include "Isoline.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include "StyleSheet.h"
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
namespace
{
// Wind direction (degrees) from U- and V-components, missing if not available
T::ParamValue uv_direction(double uspd, double vspd, double missing)
{
  if (uspd != missing && vspd != missing)
  {
    // double wspd = sqrt(uspd * uspd + vspd * vspd);
    if (uspd != 0 || vspd != 0)
    {
      double wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
      return wdir;
    }
    return ParamValueMissing;
  }
  return ParamValueMissing;
}

// Reads point values from querydata. The ParameterOptions hold references to their
// arguments, hence the arguments are kept as members.
class QueryValueReader
{
 public:
  QueryValueReader(const Engine::Querydata::Q& theQ, const Fmi::LocalDateTime& theTime)
      : q(theQ), localdatetime(theTime)
  {
  }

  TS::Value value(const Spine::Parameter& param, const Spine::Location& loc)
  {
    auto p = Engine::Querydata::ParameterOptions(
        param, tmp, loc, tmp, tmp, *timeformatter, tmp, tmp, mylocale, tmp, nearest, 0, dummy);
    return q->value(p, localdatetime);
  }

 private:
  const Engine::Querydata::Q& q;
  Fmi::LocalDateTime localdatetime;
  std::shared_ptr<Fmi::TimeFormatter> timeformatter{Fmi::TimeFormatter::create("iso")};
  const std::string tmp;
  const std::locale& mylocale = std::locale::classic();
  const bool nearest = false;
  NFmiPoint dummy;
};

// Direction values of the parameter at the coordinates
void direction_values(QueryValueReader& reader,
                      const std::string& parameter,
                      const T::Coordinate_vec& latlonCoordinates,
                      uint gwidth,
                      uint gheight,
                      T::ParamValue_vec& gridValues)
{
  auto param = TS::ParameterFactory::instance().parse(parameter);

  uint c = 0;
  for (uint y = 0; y < gheight; y++)
  {
    for (uint x = 0; x < gwidth; x++)
    {
      Spine::Location loc(latlonCoordinates[c].x(), latlonCoordinates[c].y());

      auto res = reader.value(param, loc);
      if (const double* ptr = std::get_if<double>(&res))
      {
        auto direction = *ptr;
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

// Direction values from the U- and V-parameters at the coordinates
void uv_direction_values(QueryValueReader& reader,
                         const std::string& u_parameter,
                         const std::string& v_parameter,
                         const T::Coordinate_vec& latlonCoordinates,
                         uint gwidth,
                         uint gheight,
                         T::ParamValue_vec& gridValues)
{
  auto u_param = TS::ParameterFactory::instance().parse(u_parameter);
  auto v_param = TS::ParameterFactory::instance().parse(v_parameter);

  uint c = 0;
  for (uint y = 0; y < gheight; y++)
  {
    for (uint x = 0; x < gwidth; x++)
    {
      Spine::Location loc(latlonCoordinates[c].x(), latlonCoordinates[c].y());

      auto res_u = reader.value(u_param, loc);
      auto res_v = reader.value(v_param, loc);

      const double* ptr_u = std::get_if<double>(&res_u);
      const double* ptr_v = std::get_if<double>(&res_v);
      if (ptr_u && ptr_v)
      {
        // Note: qengine fixes orientation automatically
        gridValues.push_back(uv_direction(*ptr_u, *ptr_v, kFloatMissing));
      }
      else
      {
        gridValues.push_back(ParamValueMissing);
      }
      c++;
    }
  }
}

// Wind directions (degrees) from U- and V-components
T::ParamValue_vec uv_directions(const T::ParamValue_vec& u,
                                const T::ParamValue_vec& v,
                                std::size_t sz)
{
  T::ParamValue_vec gridValues;
  gridValues.reserve(sz);

  for (uint t = 0; t < sz; t++)
  {
    double uspd = u[t];
    double vspd = v[t];
    gridValues.push_back(uv_direction(uspd, vspd, ParamValueMissing));
  }
  return gridValues;
}

// Converting WKB lines to OGRGeometries
std::vector<OGRGeometryPtr> wkb_lines(const T::ByteData_vec& streamlines)
{
  std::vector<OGRGeometryPtr> geoms;
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

}  // namespace

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

    JsonTools::remove_string(paraminfo.parameter, theJson, "parameter");
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

std::vector<OGRGeometryPtr> StreamLayer::getStreams(const State& theState)
{
  try
  {
    std::vector<OGRGeometryPtr> geoms;
    if (paraminfo.source == std::string("grid"))
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
 * \brief Add the direction parameter or the U and V parameters to the grid query
 */
// ----------------------------------------------------------------------

void StreamLayer::addStreamParameters(const Engine::Grid::Engine& gridEngine,
                                      const std::string& producerName,
                                      T::AttributeList& attributeList,
                                      QueryServer::Query& query)
{
  if (!paraminfo.parameter.empty())
  {
    std::string param = gridEngine.getParameterString(producerName, paraminfo.parameter);
    attributeList.addAttribute("param", param);

    if (!projection.projectionParameter)
      projection.projectionParameter = param;

    if (param == paraminfo.parameter && query.mProducerNameList.empty())
    {
      gridEngine.getProducerNameList(producerName, query.mProducerNameList);
      if (query.mProducerNameList.empty())
        query.mProducerNameList.push_back(producerName);
    }
  }
  else if (u_parameter && v_parameter)
  {
    std::string u_param = gridEngine.getParameterString(producerName, *u_parameter);
    std::string v_param = gridEngine.getParameterString(producerName, *v_parameter);

    attributeList.addAttribute("param", u_param + "," + v_param);

    if (!projection.projectionParameter)
      projection.projectionParameter = u_param;

    if (u_param == *u_parameter && query.mProducerNameList.empty())
    {
      gridEngine.getProducerNameList(producerName, query.mProducerNameList);
      if (query.mProducerNameList.empty())
        query.mProducerNameList.push_back(producerName);
    }
  }
  else
  {
    throw Fmi::Exception(BCP,
                         "Missing 'parameter' (= direction) or 'u' / 'v' vector definitions");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the streamline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> StreamLayer::getStreamsGrid(const State& theState)
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

    std::string producerName = gridEngine->getProducerName(*paraminfo.producer);
    std::string wkt = *projection.crs;

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
        wkt = projection.getCRS().WKT();

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

    addStreamParameters(*gridEngine, producerName, attributeList, *originalGridQuery);

    add_time_attributes(attributeList, getValidTime(), origintime);

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery, attributeList);

    // Fullfilling information into the query object.

    for (auto& param : originalGridQuery->mQueryParameterList)
    {
      param.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      param.mType = QueryServer::QueryParameter::Type::Vector;
      param.mFlags = QueryServer::QueryParameter::Flags::ReturnCoordinates;

      set_query_parameter_selections(param, paraminfo);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    add_grid_size_attributes(*originalGridQuery, projection);

    if (wkt == "data")
      add_data_bbox_attribute(*originalGridQuery, projection);

    originalGridQuery->mAttributeList.setAttribute(
        "grid.coordinateType",
        Fmi::to_string(static_cast<int>(T::CoordinateTypeValue::LATLON_COORDINATES)));

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // Extracting the projection information from the query result.

    update_projection_size(projection, *query);

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    update_data_projection(projection, *query);

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
      uint sz1 = query->mQueryParameterList[0].mValueList[0]->mValueVector.size();
      uint sz2 = query->mQueryParameterList[1].mValueList[0]->mValueVector.size();

      if (sz1 == sz && sz2 == sz)
      {
        T::ParamValue_vec gridValues =
            uv_directions(query->mQueryParameterList[0].mValueList[0]->mValueVector,
                          query->mQueryParameterList[1].mValueList[0]->mValueVector,
                          sz);
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

    geoms = wkb_lines(streamlines);
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

    if (paraminfo.level)
    {
      if (!q)
        throw Fmi::Exception(BCP, "Cannot generate streamlines without gridded level data");

      if (!q->selectLevel(*paraminfo.level))
        throw Fmi::Exception(
            BCP, "Level value " + Fmi::to_string(*paraminfo.level) + " is not available!");
    }

    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate streamlines without gridded data");

    T::AttributeList attributeList;
    T::Coordinate_svec latlonCoordinates;
    uint gwidth = 0;
    uint gheight = 0;

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
      attributeList.addAttribute("grid.width", Fmi::to_string(*projection.xsize));

    if (projection.ysize)
      attributeList.addAttribute("grid.height", Fmi::to_string(*projection.ysize));

    // Getting coordinates for the new grid.
    Identification::gridDef.getGridLatLonCoordinatesByGeometry(
        attributeList, latlonCoordinates, gwidth, gheight);

    T::ParamValue_vec gridValues;
    T::ByteData_vec streamlines;
    std::size_t sz = gwidth * gheight;
    gridValues.reserve(sz);

    auto valid_time_period = getValidTimePeriod();
    NFmiMetTime met_time = valid_time_period.begin();
    QueryValueReader reader(q, Fmi::LocalDateTime(met_time, Fmi::TimeZonePtr::utc));

    // Fetching values for the new grid.

    if (!paraminfo.parameter.empty())
      direction_values(
          reader, paraminfo.parameter, *latlonCoordinates, gwidth, gheight, gridValues);
    else if (u_parameter && v_parameter)
      uv_direction_values(
          reader, *u_parameter, *v_parameter, *latlonCoordinates, gwidth, gheight, gridValues);
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

      geoms = wkb_lines(streamlines);
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

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "StreamLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
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

    if (!paraminfo.parameter.empty())
      objectKey = "streamline:" + paraminfo.parameter + ":" + qid;
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
      const OGRGeometryPtr& geom = geoms[i];

      if (geom && geom->IsEmpty() == 0)
      {
        // Store the path with unique QID
        std::string iri =
            qid + (qid.empty() ? "" : ".") + Fmi::to_string(i);  // isoline.getQid(theState);

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
    theGlobals["bbox"] = Fmi::to_string(box.xmin()) + "," + Fmi::to_string(box.ymin()) + "," +
                         Fmi::to_string(box.xmax()) + "," + Fmi::to_string(box.ymax());
    theGlobals["objects"][objectKey] = object_cdt;
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -(int)precision);

    // We created only this one layer
    group_cdt["cdata"] = useOut.str();
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void StreamLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  if (theState.isObservation(paraminfo.producer))
    return;
  if (!paraminfo.parameter.empty())
  {
    ParameterInfo info(paraminfo.parameter);
    info.producer = paraminfo.producer;
    info.level = paraminfo.level;
    add(infos, info);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo
 */
// ----------------------------------------------------------------------

void StreamLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
  // TODO();
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

    if (paraminfo.source != std::string("grid"))
      Fmi::hash_combine(hash, getModelHashValueOrEmpty(theState));

    // The hash value of the grid producer is already added
    // in the "Properties" class.

    Fmi::hash_combine(hash, countParameterHash(theState, paraminfo.parameter));
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
