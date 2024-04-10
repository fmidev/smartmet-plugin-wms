//======================================================================

#include "IsobandLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoband.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include "StyleSheet.h"
#include "ValueTools.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/observation/Settings.h>
#include <engines/querydata/Model.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>
#include <spine/Convenience.h>
#include <spine/Json.h>
#include <spine/Parameter.h>
#include <timeseries/ParameterFactory.h>
#include <timeseries/ParameterTools.h>
#include <trax/InterpolationType.h>
#include <limits>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
std::string format_auto(const Isoband& isoband, const std::string& pattern)
{
  if (isoband.lolimit)
  {
    if (isoband.hilimit)
      return fmt::format(pattern, *isoband.lolimit, *isoband.hilimit);
    return fmt::format(pattern, *isoband.lolimit, "inf");
  }

  if (isoband.hilimit)
    return fmt::format(pattern, "-inf", *isoband.hilimit);

  return fmt::format(pattern, "nan", "nan");
}

void apply_autoqid(std::vector<Isoband>& isobands, const std::string& pattern)
{
  for (auto& isoband : isobands)
  {
    if (!pattern.empty() && isoband.qid.empty())
      isoband.qid = format_auto(isoband, pattern);

    boost::replace_all(isoband.qid, ".", ",");  // replace decimal dots with ,
  }
}

// Generate a class attribute for those missing one
void apply_autoclass(std::vector<Isoband>& isobands, const std::string& pattern)
{
  if (pattern.empty())
    return;

  for (auto& isoband : isobands)
  {
    if (isoband.attributes.value("class").empty())
    {
      auto name = format_auto(isoband, pattern);
      boost::replace_all(name, ".", ",");  // replace decimal dots with ,
      isoband.attributes.add("class", name);
    }
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void IsobandLayer::init(Json::Value& theJson,
                        const State& theState,
                        const Config& theConfig,
                        const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);
    precision = theState.getPrecision("isoband");

    if (theState.getType() == "topojson")
      precision = theState.getPrecision("topojson");

    // Extract member values

    JsonTools::remove_string(parameter, theJson, "parameter");

    auto json = JsonTools::remove(theJson, "isobands");
    if (!json.isNull())
      JsonTools::extract_array("isobands", isobands, json, theConfig);

    std::string autoqid;
    JsonTools::remove_string(autoqid, theJson, "autoqid");
    apply_autoqid(isobands, autoqid);

    std::string autoclass;
    JsonTools::remove_string(autoclass, theJson, "autoclass");
    apply_autoclass(isobands, autoclass);

    JsonTools::remove_string(interpolation, theJson, "interpolation");
    JsonTools::remove_int(extrapolation, theJson, "extrapolation");

    json = JsonTools::remove(theJson, "smoother");
    smoother.init(json, theConfig);

    JsonTools::remove_double(precision, theJson, "precision");
    JsonTools::remove_double(minarea, theJson, "minarea");
    JsonTools::remove_string(areaunit, theJson, "areaunit");
    JsonTools::remove_string(unit_conversion, theJson, "unit_conversion");
    JsonTools::remove_double(multiplier, theJson, "multiplier");
    JsonTools::remove_double(offset, theJson, "offset");

    JsonTools::remove_bool(closed_range, theJson, "closed_range");
    JsonTools::remove_bool(strict, theJson, "strict");
    JsonTools::remove_bool(validate, theJson, "validate");
    JsonTools::remove_bool(desliver, theJson, "desliver");

    json = JsonTools::remove(theJson, "outside");
    if (!json.isNull())
    {
      outside.reset(Map());
      outside->init(json, theConfig);
    }

    json = JsonTools::remove(theJson, "inside");
    if (!json.isNull())
    {
      inside.reset(Map());
      inside->init(json, theConfig);
    }

    json = JsonTools::remove(theJson, "sampling");
    sampling.init(json, theConfig);

    json = JsonTools::remove(theJson, "filter");
    filter.init(json);

    json = JsonTools::remove(theJson, "intersect");
    intersections.init(json, theConfig);

    json = JsonTools::remove(theJson, "heatmap");
    heatmap.init(json, theConfig);

    if (areaunit != "km^2" && areaunit != "px^2")
      throw Fmi::Exception(BCP, "Unknown areaunit '" + areaunit + '"');
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

boost::shared_ptr<Engine::Querydata::QImpl> IsobandLayer::buildHeatmap(
    const Spine::Parameter& theParameter, const Fmi::DateTime& theTime, State& theState)
{
  try
  {
    if (!isFlashOrMobileProducer(*producer))
      throw Fmi::Exception(BCP, "Heatmap requires flash or mobile data!");

    auto valid_time_period = getValidTimePeriod();
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    Engine::Observation::Settings settings;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();
    settings.starttimeGiven = true;
    settings.stationtype = *producer;
    settings.timezone = "UTC";
    settings.localTimePool = theState.getLocalTimePool();

    // Get actual data (flash coordinates plus parameter column values)
    auto& obsengine = theState.getObsEngine();
    settings.parameters.push_back(TS::makeParameter("longitude"));
    settings.parameters.push_back(TS::makeParameter("latitude"));
    settings.parameters.push_back(TS::makeParameter(*parameter));

    settings.boundingBox = getClipBoundingBox(box, crs);

    auto result = obsengine.values(settings);

    // Establish new projection and the required grid size of the desired resolution

    std::unique_ptr<NFmiArea> newarea(NFmiArea::CreateFromBBox(
        crs, NFmiPoint(box.xmin(), box.ymin()), NFmiPoint(box.xmax(), box.ymax())));

    double datawidth = std::abs(newarea->WorldXYWidth());  // in native units
    double dataheight = std::abs(newarea->WorldXYHeight());

    // The test against 360 is due to legacy NFmiLatLonArea, which is sort of geographic
    // but WorldXYWidth returns equidistant cylindrical width instead. This kludge effectively
    // breaks metric projection heatmaps for areas smaller than 360 meters, but such analysis
    // is highly unlikely to happen.

    if (newarea->SpatialReference().isGeographic() && datawidth < 360)
    {
      datawidth *= kRearth * kPii / 180 / 1000;  // degrees to kilometers
      dataheight *= kRearth * kPii / 180 / 1000;
    }
    else
    {
      datawidth /= 1000;  // meters to kilometers
      dataheight /= 1000;
    }

    unsigned int width = lround(datawidth / *heatmap.resolution);
    unsigned int height = lround(dataheight / *heatmap.resolution);

    if (width * height > heatmap.max_points)
      throw Fmi::Exception(
          BCP,
          (std::string("Heatmap too big (") + Fmi::to_string(width * height) + " points, max " +
           Fmi::to_string(heatmap.max_points) + "), increase resolution"));

    // Must use at least two grid points, value 1 would cause a segmentation fault in here

    width = std::max(width, 2U);
    height = std::max(height, 2U);

    NFmiGrid grid(newarea.get(), width, height);
    std::unique_ptr<heatmap_t, void (*)(heatmap_t*)> hm(nullptr, heatmap_free);
    unsigned radius = 0;

    if (result)
    {
      const auto& values = *result;

      if (!values.empty())
      {
        const auto nrows = values[0].size();

        hm.reset(heatmap_new(width, height));
        if (!hm)
          throw Fmi::Exception(BCP, "Heatmap allocation failed");

        radius = lround(*heatmap.radius / *heatmap.resolution);
        if (radius == 0)
          radius = 1;

        auto hms = heatmap.getStamp(radius);
        if (!hms)
          throw Fmi::Exception(BCP, "Heatmap stamp generation failed");

        for (std::size_t row = 0; row < nrows; ++row)
        {
          double lon = get_double(values.at(0).at(row));
          double lat = get_double(values.at(1).at(row));

          // The parameter value could be used to add weighted points
          //
          // double value = get_double(values.at(2).at(row));

          // Convert latlon to world coordinate if needed

          double x = lon;
          double y = lat;
          if (crs.isGeographic() == 0)
          {
            auto xy = newarea->LatLonToWorldXY(NFmiPoint(x, y));
            x = xy.X();
            y = xy.Y();
          }

          // To pixel coordinate
          box.transform(x, y);

          // Skip if not inside desired area
          if (Properties::inside(box, x, y))
          {
            NFmiPoint gridPoint = grid.LatLonToGrid(lon, lat);
            heatmap_add_point_with_stamp(
                hm.get(), lround(gridPoint.X()), lround(gridPoint.Y()), hms.get());
          }
        }
      }
    }

    // Establish the new descriptors

    NFmiLevelBag lbag;
    lbag.AddLevel(NFmiLevel(kFmiAnyLevelType, 0));

    NFmiVPlaceDescriptor vdesc(lbag);

    NFmiParamBag pbag;
    pbag.Add(NFmiParam(1));
    NFmiParamDescriptor pdesc(pbag);

    NFmiTimeList tlist;
    tlist.Add(new NFmiMetTime(theTime));  // NOLINT(cppcoreguidelines-owning-memory)
    NFmiTimeDescriptor tdesc(theTime, tlist);

    NFmiHPlaceDescriptor hdesc(grid);

    // Then create the new querydata

    NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc);
    boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
    if (data == nullptr)
      throw Fmi::Exception(BCP, "Failed to create heatmap");

    NFmiFastQueryInfo dstinfo(data.get());
    dstinfo.First();

    if (result && hm && hm->buf != nullptr)
    {
      float* v = hm->buf;

      for (dstinfo.ResetLocation(); dstinfo.NextLocation();)
        dstinfo.FloatValue(*v++);
    }

    // Return the new Q but with a new hash value

    std::size_t hash = 0;
    Fmi::hash_combine(hash, Fmi::hash_value(theParameter.originalName()));
    Fmi::hash_combine(hash, Fmi::hash_value(valid_time_period.begin()));
    Fmi::hash_combine(hash, Fmi::hash_value(valid_time_period.end()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.xmin()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.ymin()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.xmax()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.ymax()));
    Fmi::hash_combine(hash, Dali::hash_value(heatmap, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(radius));

    char* tmp = nullptr;
    crs.get()->exportToWkt(&tmp);
    Fmi::hash_combine(hash, Fmi::hash_value(tmp));
    CPLFree(tmp);

    auto model = boost::make_shared<Engine::Querydata::Model>(data, hash);
    return boost::make_shared<Engine::Querydata::QImpl>(model);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void IsobandLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    if (isobands.empty())
      return;

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    if (source && *source == "grid")
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!")
        .addParameter("qid", qid)
        .addParameter("Producer", *producer)
        .addParameter("Parameter", *parameter);
  }
}

void IsobandLayer::generate_gridEngine(CTPP::CDT& theGlobals,
                                       CTPP::CDT& theLayersCdt,
                                       State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    if (!parameter)
      throw Fmi::Exception(BCP, "Parameter not set for isoband-layer");

    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Establish the parameter
    //
    // Heatmap does not use the parameter currently (only flash or mobile coordinates)
    // bool allowUnknownParam = (theState.isObservation(producer) &&
    //                          isFlashOrMobileProducer(*producer) && heatmap.resolution);

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);

    // Establish the valid time
    auto valid_time = getValidTime();

    T::ParamValue_vec contourLowValues;
    T::ParamValue_vec contourHighValues;
    for (const Isoband& isoband : isobands)
    {
      if (isoband.lolimit)
        contourLowValues.push_back(*isoband.lolimit);
      else
        contourLowValues.push_back(-1000000000.0);

      if (isoband.hilimit)
        contourHighValues.push_back(*isoband.hilimit);
      else
        contourHighValues.push_back(1000000000);
    }

    // Alter units if requested
    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

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

      // std::cout << wkt << "\n";

      // Adding the bounding box information into the query.

      const auto& box = projection.getBox();
      const auto clipbox = getClipBox(box);

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      auto bbox = fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      // bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
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

    std::string pName = *parameter;
    auto pos = pName.find(".raw");
    if (pos != std::string::npos)
    {
      attributeList.addAttribute("areaInterpolationMethod",
                                 std::to_string(T::AreaInterpolationMethod::Linear));
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

    for (auto& p : originalGridQuery->mQueryParameterList)
    {
      p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      p.mType = QueryServer::QueryParameter::Type::Isoband;
      p.mContourLowValues = contourLowValues;
      p.mContourHighValues = contourHighValues;

      if (geometryId)
        p.mGeometryId = *geometryId;

      if (levelId)
        p.mParameterLevelId = *levelId;

      if (level)
        p.mParameterLevel = C_INT(*level);

      if (forecastType)
        p.mForecastType = C_INT(*forecastType);

      if (forecastNumber)
        p.mForecastNumber = C_INT(*forecastNumber);
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

    if (projection.bboxcrs)
      originalGridQuery->mAttributeList.addAttribute("grid.bboxcrs", *projection.bboxcrs);

    if (projection.cx)
      originalGridQuery->mAttributeList.addAttribute("grid.cx", std::to_string(*projection.cx));

    if (projection.cy)
      originalGridQuery->mAttributeList.addAttribute("grid.cy", std::to_string(*projection.cy));

    if (projection.resolution)
      originalGridQuery->mAttributeList.addAttribute("grid.resolution",
                                                     std::to_string(*projection.resolution));

    if (wkt == "data" && projection.x1 && projection.y1 && projection.x2 && projection.y2)
    {
      auto bbox = fmt::format(
          "{},{},{},{}", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }

    if (smoother.size)
      originalGridQuery->mAttributeList.addAttribute("contour.smooth.size",
                                                     std::to_string(*smoother.size));

    if (smoother.degree)
      originalGridQuery->mAttributeList.addAttribute("contour.smooth.degree",
                                                     std::to_string(*smoother.degree));

    originalGridQuery->mAttributeList.addAttribute("contour.extrapolation",
                                                   std::to_string(extrapolation));

    if (extrapolation)
      originalGridQuery->mAttributeList.addAttribute("contour.multiplier",
                                                     std::to_string(*multiplier));

    if (offset)
      originalGridQuery->mAttributeList.addAttribute("contour.offset", std::to_string(*offset));

    originalGridQuery->mAttributeList.setAttribute(
        "contour.coordinateType",
        std::to_string(static_cast<int>(T::CoordinateTypeValue::ORIGINAL_COORDINATES)));
    // query.mAttributeList.setAttribute("contour.coordinateType",std::to_string(T::CoordinateTypeValue::LATLON_COORDINATES));
    // query.mAttributeList.setAttribute("contour.coordinateType",std::to_string(T::CoordinateTypeValue::GRID_COORDINATES));

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    // query.print(std::cout,0,0);

    // Converting the returned WKB-isolines into OGRGeometry objects.

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
#if 0
          int width = 3600; //atoi(query.mAttributeList.getAttributeValue("grid.width"));
          int height = 1800; // atoi(query.mAttributeList.getAttributeValue("grid.height"));

          if (width > 0 &&  height > 0)
          {
            double mp = 10;
            ImagePaint imagePaint(width,height,0xFFFFFF,false,true);

            // ### Painting contours into the image:

            if (param.mType == QueryServer::QueryParameter::Type::Isoband)
            {
              if (!val->mValueData.empty())
              {
                uint c = 250;
                uint step = 250 / val->mValueData.size();

                for (const auto & it: val->mValueData)
                {
                  printf("Contour %lu\n",it.size());
                  uint col = (c << 16) + (c << 8) + c;
                  imagePaint.paintWkb(mp,mp,180,90,it,col);
                  c = c - step;
                }
              }
            }
            else
            {
              imagePaint.paintWkb(mp,mp,180,90,val->mValueData,0x00);
            }

            // ### Saving the image and releasing the image data:

            char fname[200];
            sprintf(fname,"/tmp/contour_%llu.jpg",getTime());
            imagePaint.saveJpgImage(fname);
          }
#endif
        }
      }
    }

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

    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    if (minarea)
    {
      auto area = *minarea;
      if (areaunit == "px^2")
        area = box.areaFactor() * area;

      originalGridQuery->mAttributeList.addAttribute("contour.minArea", std::to_string(area));
    }

    if (wkt == "data")
      return;

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape;
    OGRGeometryPtr outshape;
    if (inside)
    {
      inshape = gis.getShape(&crs, inside->options);
      if (!inshape)
        throw Fmi::Exception(BCP, "Received empty inside-shape from database!");

      inshape.reset(Fmi::OGR::polyclip(*inshape, clipbox));
    }
    if (outside)
    {
      outshape = gis.getShape(&crs, outside->options);
      if (outshape)
        outshape.reset(Fmi::OGR::polyclip(*outshape, clipbox));
    }

    // Logical operations with isobands are initialized before hand

    intersections.init(producer, gridEngine, projection, valid_time, theState);

    CTPP::CDT object_cdt;
    std::string objectKey = "isoband:" + *parameter + ":" + qid;
    object_cdt["objectKey"] = objectKey;

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate isobands as use tags statements inside <g>..</g>

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    // Add attributes to the group, not the isobands
    theState.addAttributes(theGlobals, group_cdt, attributes);

    for (unsigned int i = 0; i < geoms.size(); i++)
    {
      OGRGeometryPtr geom = geoms[i];
      if (geom && geom->IsEmpty() == 0)
      {
        OGRGeometryPtr geom2(Fmi::OGR::polyclip(*geom, clipbox));
        const Isoband& isoband = isobands[i];

        // Do intersections if so requested

        if (geom2 && geom2->IsEmpty() == 0 && inshape)
          geom2.reset(geom2->Intersection(inshape.get()));

        if (geom2 && geom2->IsEmpty() == 0 && outshape)
          geom2.reset(geom2->Difference(outshape.get()));

        // Intersect with data too

        geom2 = intersections.intersect(geom2);

        // Finally produce output if we still have something left
        if (geom2 && geom2->IsEmpty() == 0)
        {
          // Store the path with unique ID
          std::string iri = qid + (qid.empty() ? "" : ".") + isoband.getQid(theState);

          if (!theState.addId(iri))
            throw Fmi::Exception(BCP, "Non-unique ID assigned to isoband").addParameter("ID", iri);

          CTPP::CDT isoband_cdt(CTPP::CDT::HASH_VAL);

          std::string arcNumbers;
          std::string arcCoordinates;
          std::string pointCoordinates;

          isoband_cdt["iri"] = iri;
          isoband_cdt["time"] = Fmi::to_iso_extended_string(valid_time);
          isoband_cdt["parameter"] = *parameter;
          pointCoordinates = Geometry::toString(*geom2,
                                                theState.getType(),
                                                box,
                                                crs,
                                                precision,
                                                theState.arcHashMap,
                                                theState.arcCounter,
                                                arcNumbers,
                                                arcCoordinates);

          if (!pointCoordinates.empty())
            isoband_cdt["data"] = pointCoordinates;

          isoband_cdt["type"] = Geometry::name(*geom2, theState.getType());
          isoband_cdt["layertype"] = "isoband";

          // Use null to indicate unset values in GeoJSON
          if (isoband.lolimit)
            isoband_cdt["lolimit"] = *isoband.lolimit;
          else
            isoband_cdt["lolimit"] = "null";

          if (isoband.hilimit)
            isoband_cdt["hilimit"] = *isoband.hilimit;
          else
            isoband_cdt["hilimit"] = "null";

          theState.addPresentationAttributes(isoband_cdt, css, attributes, isoband.attributes);

          if (theState.getType() == "topojson")
          {
            if (!arcNumbers.empty())
              isoband_cdt["arcs"] = arcNumbers;
            ;

            if (!arcCoordinates.empty())
            {
              CTPP::CDT arc_cdt(CTPP::CDT::HASH_VAL);
              arc_cdt["data"] = arcCoordinates;
              theGlobals["arcs"][theState.insertCounter] = arc_cdt;
              theState.insertCounter++;
            }
            object_cdt["paths"][iri] = isoband_cdt;
          }
          else
          {
            theGlobals["paths"][iri] = isoband_cdt;
          }

          // Add the SVG use element
          CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
          tag_cdt["start"] = "<use";
          tag_cdt["end"] = "/>";
          theState.addAttributes(theGlobals, tag_cdt, isoband.attributes);
          tag_cdt["attributes"]["xlink:href"] = "#" + iri;
          group_cdt["tags"].PushBack(tag_cdt);
        }
      }
    }

    theGlobals["bbox"] = std::to_string(box.xmin()) + "," + std::to_string(box.ymin()) + "," +
                         std::to_string(box.xmax()) + "," + std::to_string(box.ymax());
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -static_cast<int>(precision));

    theGlobals["objects"][objectKey] = object_cdt;

    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void IsobandLayer::generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Establish the data
    auto q = getModel(theState);

    if (q && !(q->isGrid()))
      throw Fmi::Exception(BCP, "Isoband-layer can't use point data!");

    // Establish the parameter
    //
    // Heatmap does not use the parameter currently (only flash or mobile coordinates)
    bool allowUnknownParam = (theState.isObservation(producer) &&
                              isFlashOrMobileProducer(*producer) && heatmap.resolution);

    if (!parameter)
      throw Fmi::Exception(BCP, "Parameter not set for isoband-layer!");
    auto param = TS::ParameterFactory::instance().parse(*parameter, allowUnknownParam);

    // Establish the valid time

    auto valid_time = getValidTime();

    // Establish the level

    if (q && !q->firstLevel())
      throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

    if (level)
    {
      if (!q)
        throw Fmi::Exception(BCP, "Cannot generate isobands without gridded level data");

      if (!q->selectLevel(*level))
        throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available!");
    }

    // Get projection details

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Sample to higher resolution if necessary

    auto sampleresolution = sampling.getResolution(projection);
    if (sampleresolution)
    {
      if (!q)
        throw Fmi::Exception(BCP, "Cannot resample without gridded data");

      if (heatmap.resolution)
        throw Fmi::Exception(BCP, "Isoband-layer can't use both sampling and heatmap!");

      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer2;
      if (theState.useTimer())
      {
        std::string report2 = "IsobandLayer::resample finished in %t sec CPU, %w sec real\n";
        timer2 = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report2);
      }

      auto demdata = theState.getGeoEngine().dem();
      auto landdata = theState.getGeoEngine().landCover();
      if (!demdata || !landdata)
        throw Fmi::Exception(BCP,
                             "Resampling data requires DEM and land cover data to be available!");

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
    else if (heatmap.resolution)
    {
      q = buildHeatmap(param, valid_time, theState);
    }
    else if (theState.isObservation(producer))
      throw Fmi::Exception(
          BCP, "Can't produce isobandlayer from observation data without heatmap configuration!");

    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate isobands without gridded data");

    // Logical operations with maps require shapes

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape;
    OGRGeometryPtr outshape;
    if (inside)
    {
      inshape = gis.getShape(&crs, inside->options);
      if (!inshape)
        throw Fmi::Exception(BCP, "Received empty inside-shape from database!");

      inshape.reset(Fmi::OGR::polyclip(*inshape, clipbox));
    }
    if (outside)
    {
      outshape = gis.getShape(&crs, outside->options);
      if (outshape)
        outshape.reset(Fmi::OGR::polyclip(*outshape, clipbox));
    }

    // Logical operations with isobands are initialized before hand

    intersections.init(producer, projection, valid_time, theState);

    // Calculate the isobands and store them into the template engine

    const auto& contourer = theState.getContourEngine();
    std::vector<Engine::Contour::Range> limits;
    limits.reserve(isobands.size());
    for (const Isoband& isoband : isobands)
      limits.emplace_back(isoband.lolimit, isoband.hilimit);

    Engine::Contour::Options options(param, valid_time, limits);
    options.level = level;
    options.bbox = Fmi::BBox(box);

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    if (multiplier || offset)
      options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

    options.minarea = minarea;
    if (minarea)
    {
      if (areaunit == "px^2")
        options.minarea = box.areaFactor() * *minarea;
    }

    options.filter_size = smoother.size;
    options.filter_degree = smoother.degree;

    options.extrapolation = extrapolation;

    if (interpolation == "linear")
      options.interpolation = Trax::InterpolationType::Linear;
    else if (interpolation == "nearest" || interpolation == "discrete" ||
             interpolation == "midpoint")
      options.interpolation = Trax::InterpolationType::Midpoint;
    else if (interpolation == "logarithmic")
      options.interpolation = Trax::InterpolationType::Logarithmic;
    else
      throw Fmi::Exception(BCP, "Unknown isoband interpolation method '" + interpolation + "'!");

    options.closed_range = closed_range;
    options.strict = strict;
    options.validate = validate;
    options.desliver = desliver;

    // Do the actual contouring, either full grid or just
    // a sampled section

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    Fmi::hash_combine(valueshash, options.data_hash_value());

    // Select the data

    if (heatmap.resolution)
    {
      // Heatmap querydata has just 1 fixed parameter (1/pressure)
      options.parameter = TS::ParameterFactory::instance().parse("1");
    }

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

    // Avoid reprojecting data when sampling has been used for better speed (and accuracy)
    CoordinatesPtr coords;
    if (sampleresolution)
      coords = qEngine.getWorldCoordinates(q);
    else
      coords = qEngine.getWorldCoordinates(q, crs);

    std::vector<OGRGeometryPtr> geoms =
        contourer.contour(qhash, crs, *matrix, *coords, clipbox, options);

    filter.bbox(box);
    filter.apply(geoms, true);

    CTPP::CDT object_cdt;
    std::string objectKey = "isoband:" + *parameter + ":" + qid;
    object_cdt["objectKey"] = objectKey;

    // Update the globals

    // if (css)
    //{
    //  std::string name = theState.getCustomer() + "/" + *css;
    //  theGlobals["css"][name] = theState.getStyle(*css);
    // }

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate isobands as use tags statements inside <g>..</g>

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    // Add attributes to the group, not the isobands
    theState.addAttributes(theGlobals, group_cdt, attributes);

    for (unsigned int i = 0; i < geoms.size(); i++)
    {
      OGRGeometryPtr geom = geoms[i];

      if (geom && geom->IsEmpty() == 0)
      {
        OGRGeometryPtr geom2(Fmi::OGR::polyclip(*geom, clipbox));

        const Isoband& isoband = isobands[i];

        // Do intersections if so requested

        if (geom2 && geom2->IsEmpty() == 0 && inshape)
          geom2.reset(geom2->Intersection(inshape.get()));

        if (geom2 && geom2->IsEmpty() == 0 && outshape)
          geom2.reset(geom2->Difference(outshape.get()));

        // Intersect with data too

        geom2 = intersections.intersect(geom2);

        // Finally produce output if we still have something left
        if (geom2 && geom2->IsEmpty() == 0)
        {
          // Store the path with unique ID
          std::string iri = qid + (qid.empty() ? "" : ".") + isoband.getQid(theState);

          if (!theState.addId(iri))
            throw Fmi::Exception(BCP, "Non-unique ID assigned to isoband").addParameter("ID", iri);

          std::string arcNumbers;
          std::string arcCoordinates;
          std::string pointCoordinates;

          CTPP::CDT isoband_cdt(CTPP::CDT::HASH_VAL);
          isoband_cdt["iri"] = iri;
          isoband_cdt["time"] = Fmi::to_iso_extended_string(valid_time);
          isoband_cdt["parameter"] = *parameter;

          pointCoordinates = Geometry::toString(*geom2,
                                                theState.getType(),
                                                box,
                                                crs,
                                                precision,
                                                theState.arcHashMap,
                                                theState.arcCounter,
                                                arcNumbers,
                                                arcCoordinates);

          if (!pointCoordinates.empty())
            isoband_cdt["data"] = pointCoordinates;

          // isoband_cdt["data"] = Geometry::toString(*geom2, theState.getType(), box, crs,
          // precision);

          isoband_cdt["type"] = Geometry::name(*geom2, theState.getType());
          isoband_cdt["layertype"] = "isoband";

          // Use null to indicate unset values in GeoJSON
          if (isoband.lolimit)
            isoband_cdt["lolimit"] = *isoband.lolimit;
          else
            isoband_cdt["lolimit"] = "null";

          if (isoband.hilimit)
            isoband_cdt["hilimit"] = *isoband.hilimit;
          else
            isoband_cdt["hilimit"] = "null";

          theState.addPresentationAttributes(isoband_cdt, css, attributes, isoband.attributes);

          if (theState.getType() == "topojson")
          {
            if (!arcNumbers.empty())
              isoband_cdt["arcs"] = arcNumbers;
            ;

            if (!arcCoordinates.empty())
            {
              CTPP::CDT arc_cdt(CTPP::CDT::HASH_VAL);
              arc_cdt["data"] = arcCoordinates;
              theGlobals["arcs"][theState.insertCounter] = arc_cdt;
              theState.insertCounter++;
            }
            object_cdt["paths"][iri] = isoband_cdt;
          }
          else
          {
            theGlobals["paths"][iri] = isoband_cdt;
          }

          // theGlobals["paths"][iri] = isoband_cdt;

          // Add the SVG use element
          CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
          tag_cdt["start"] = "<use";
          tag_cdt["end"] = "/>";
          theState.addAttributes(theGlobals, tag_cdt, isoband.attributes);
          tag_cdt["attributes"]["xlink:href"] = "#" + iri;
          group_cdt["tags"].PushBack(tag_cdt);
        }
      }
    }
    theGlobals["objects"][objectKey] = object_cdt;
    theGlobals["bbox"] = std::to_string(box.xmin()) + "," + std::to_string(box.ymin()) + "," +
                         std::to_string(box.xmax()) + "," + std::to_string(box.ymax());
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -static_cast<int>(precision));

    // We created only this one layer
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

void IsobandLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
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

std::size_t IsobandLayer::hash_value(const State& theState) const
{
  try
  {
    // if (source && *source == "grid")
    // return Fmi::bad_hash;

    auto hash = Layer::hash_value(theState);

    if (!theState.isObservation(producer) && !(source && *source == "grid"))
      Fmi::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

    Fmi::hash_combine(hash, Fmi::hash_value(parameter));
    Fmi::hash_combine(hash, Dali::hash_value(isobands, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(interpolation));
    Fmi::hash_combine(hash, Dali::hash_value(smoother, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(extrapolation));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Fmi::hash_value(minarea));
    Fmi::hash_combine(hash, Fmi::hash_value(areaunit));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Dali::hash_value(outside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(inside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(sampling, theState));
    Fmi::hash_combine(hash, Dali::hash_value(intersections, theState));
    Fmi::hash_combine(hash, filter.hash_value());
    Fmi::hash_combine(hash, Dali::hash_value(heatmap, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(closed_range));
    Fmi::hash_combine(hash, Fmi::hash_value(strict));
    Fmi::hash_combine(hash, Fmi::hash_value(validate));
    Fmi::hash_combine(hash, Fmi::hash_value(desliver));
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
