//======================================================================

#include "IsobandLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoband.h"
#include "Layer.h"
#include "State.h"
#include "ValueTools.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/contour/Interpolation.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/observation/Settings.h>
#include <engines/querydata/Model.h>
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
#include <spine/ParameterFactory.h>
#include <spine/ParameterTools.h>
#include <limits>

#ifndef NEW_NFMIAREA
#include <newbase/NFmiGdalArea.h>
#endif

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

void IsobandLayer::init(const Json::Value& theJson,
                        const State& theState,
                        const Config& theConfig,
                        const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);
    precision = theConfig.defaultPrecision("isoband");

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("geometryId", nulljson);
    if (!json.isNull())
      geometryId = json.asInt();

    json = theJson.get("levelId", nulljson);
    if (!json.isNull())
      levelId = json.asInt();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("forecastType", nulljson);
    if (!json.isNull())
      forecastType = json.asInt();

    json = theJson.get("forecastNumber", nulljson);
    if (!json.isNull())
      forecastNumber = json.asInt();

    auto request = theState.getRequest();

    boost::optional<std::string> v = request.getParameter("geometryId");
    if (v)
      geometryId = toInt32(*v);

    v = request.getParameter("levelId");
    if (v)
      levelId = toInt32(*v);

    v = request.getParameter("level");
    if (v)
      level = toInt32(*v);

    v = request.getParameter("forecastType");
    if (v)
      forecastType = toInt32(*v);

    v = request.getParameter("forecastNumber");
    if (v)
      forecastNumber = toInt32(*v);

    json = theJson.get("isobands", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("isobands", isobands, json, theConfig);

    json = theJson.get("interpolation", nulljson);
    if (!json.isNull())
      interpolation = json.asString();

    json = theJson.get("extrapolation", nulljson);
    if (!json.isNull())
      extrapolation = json.asInt();

    json = theJson.get("smoother", nulljson);
    if (!json.isNull())
      smoother.init(json, theConfig);

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

    json = theJson.get("heatmap", nulljson);
    if (!json.isNull())
      heatmap.init(json, theConfig);
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
    const Spine::Parameter& theParameter, const boost::posix_time::ptime& theTime, State& theState)
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

    // Get actual data (flash coordinates plus parameter column values)
    auto& obsengine = theState.getObsEngine();
    settings.parameters.push_back(Spine::makeParameter("longitude"));
    settings.parameters.push_back(Spine::makeParameter("latitude"));
    settings.parameters.push_back(Spine::makeParameter(*parameter));

    settings.boundingBox = getClipBoundingBox(box, crs);

    auto result = obsengine.values(settings);

    // Establish new projection and the required grid size of the desired resolution

#ifdef NEW_NFMIAREA
    std::unique_ptr<NFmiArea> newarea(NFmiArea::CreateFromBBox(
        crs, NFmiPoint(box.xmin(), box.ymin()), NFmiPoint(box.xmax(), box.ymax())));

    double datawidth = newarea->WorldXYWidth();  // in native units
    double dataheight = newarea->WorldXYHeight();

    if (newarea->SpatialReference().isGeographic())
    {
      datawidth *= kRearth * kPii / 180 / 1000;  // degrees to kilometers
      dataheight *= kRearth * kPii / 180 / 1000;
    }
    else
    {
      datawidth /= 1000;  // meters to kilometers
      dataheight /= 1000;
    }
#else
    auto newarea = boost::make_shared<NFmiGdalArea>(
        "FMI", *crs, box.xmin(), box.ymin(), box.xmax(), box.ymax());

    double datawidth = newarea->WorldXYWidth() / 1000.0;  // view extent in kilometers
    double dataheight = newarea->WorldXYHeight() / 1000.0;
#endif

    unsigned int width = lround(datawidth / *heatmap.resolution);
    unsigned int height = lround(dataheight / *heatmap.resolution);

    if (width * height > heatmap.max_points)
      throw Fmi::Exception(
          BCP,
          (std::string("Heatmap too big (") + Fmi::to_string(width * height) + " points, max " +
           Fmi::to_string(heatmap.max_points) + "), increase resolution"));

    // Must use at least two grid points, value 1 would cause a segmentation fault in here

    width = std::max(width, 2u);
    height = std::max(height, 2u);

    NFmiGrid grid(newarea.get(), width, height);
    std::unique_ptr<heatmap_t, void (*)(heatmap_t*)> hm(nullptr, heatmap_free);
    unsigned radius;

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
    boost::hash_combine(hash, theParameter);
    boost::hash_combine(hash, to_simple_string(valid_time_period.begin()));
    boost::hash_combine(hash, to_simple_string(valid_time_period.end()));
    boost::hash_combine(hash, box.xmin());
    boost::hash_combine(hash, box.ymin());
    boost::hash_combine(hash, box.xmax());
    boost::hash_combine(hash, box.ymax());
    boost::hash_combine(hash, Dali::hash_value(heatmap, theState));
    boost::hash_combine(hash, radius);

    char* tmp;
    crs.get()->exportToWkt(&tmp);
    boost::hash_combine(hash, tmp);
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

    if (source && *source == "grid")
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Operation failed!", nullptr);
    exception.addParameter("Producer", *producer);
    exception.addParameter("Parameter", *parameter);
    throw exception;
  }
}

void IsobandLayer::generate_gridEngine(CTPP::CDT& theGlobals,
                                       CTPP::CDT& theLayersCdt,
                                       State& theState)
{
  try
  {
    auto gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    if (!parameter)
      throw Fmi::Exception(BCP, "Parameter not set for isoband-layer");

    std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

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

      char bbox[100];

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      sprintf(bbox, "%f,%f,%f,%f", bl.X(), bl.Y(), tr.X(), tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      const auto& box = projection.getBox();
      sprintf(bbox, "%f,%f,%f,%f", box.xmin(), box.ymin(), box.xmax(), box.ymax());
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

    if (param == *parameter && originalGridQuery->mProducerNameList.size() == 0)
    {
      gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
      if (originalGridQuery->mProducerNameList.size() == 0)
        originalGridQuery->mProducerNameList.push_back(producerName);
    }

    std::string forecastTime = Fmi::to_iso_string(*time);
    attributeList.addAttribute("startTime", forecastTime);
    attributeList.addAttribute("endTime", forecastTime);
    attributeList.addAttribute("timelist", forecastTime);
    attributeList.addAttribute("timezone", "UTC");

    if (origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery, attributeList);

    // Fullfilling information into the query object.

    for (auto it = originalGridQuery->mQueryParameterList.begin();
         it != originalGridQuery->mQueryParameterList.end();
         ++it)
    {
      it->mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      it->mType = QueryServer::QueryParameter::Type::Isoband;
      it->mContourLowValues = contourLowValues;
      it->mContourHighValues = contourHighValues;

      if (geometryId)
        it->mGeometryId = *geometryId;

      if (levelId)
        it->mParameterLevelId = *levelId;

      if (level)
        it->mParameterLevel = C_INT(*level);

      if (forecastType)
        it->mForecastType = C_INT(*forecastType);

      if (forecastNumber)
        it->mForecastNumber = C_INT(*forecastNumber);
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
      char bbox[100];
      sprintf(bbox, "%f,%f,%f,%f", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }

    if (smoother.size)
      originalGridQuery->mAttributeList.addAttribute("contour.smooth.size",
                                                     std::to_string(*smoother.size));

    if (smoother.degree)
      originalGridQuery->mAttributeList.addAttribute("contour.smooth.degree",
                                                     std::to_string(*smoother.degree));

    if (minarea)
      originalGridQuery->mAttributeList.addAttribute("contour.minArea", std::to_string(*minarea));

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
    for (auto param = query->mQueryParameterList.begin(); param != query->mQueryParameterList.end();
         ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        if ((*val)->mValueData.size() > 0)
        {
          uint c = 0;
          for (auto wkb = (*val)->mValueData.begin(); wkb != (*val)->mValueData.end(); ++wkb)
          {
            unsigned char* cwkb = reinterpret_cast<unsigned char*>(wkb->data());
            OGRGeometry* geom = nullptr;
            OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb->size());
            auto geomPtr = OGRGeometryPtr(geom);
            geoms.push_back(geomPtr);
            c++;
          }
#if 0
          int width = 3600; //atoi(query.mAttributeList.getAttributeValue("grid.width"));
          int height = 1800; // atoi(query.mAttributeList.getAttributeValue("grid.height"));

          if (width > 0 &&  height > 0)
          {
            double mp = 10;
            ImagePaint imagePaint(width,height,0xFFFFFF,false,true);

            // ### Painting contours into the image:

            if (param->mType == QueryServer::QueryParameter::Type::Isoband)
            {
              if (val->mValueData.size() > 0)
              {
                uint c = 250;
                uint step = 250 / val->mValueData.size();

                for (auto it = val->mValueData.begin(); it != val->mValueData.end(); ++it)
                {
                  printf("Contour %lu\n",it->size());
                  uint col = (c << 16) + (c << 8) + c;
                  imagePaint.paintWkb(mp,mp,180,90,*it,col);
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
        projection.xsize = atoi(widthStr);

      if (heightStr != nullptr)
        projection.ysize = atoi(heightStr);
    }

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    if (*projection.crs == "data")
    {
      const char* crsStr = query->mAttributeList.getAttributeValue("grid.crs");
      const char* proj4Str = query->mAttributeList.getAttributeValue("grid.proj4");
      if (proj4Str != nullptr &&  strstr(proj4Str,"+lon_wrap") != nullptr )
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

    if (wkt == "data")
      return;

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape, outshape;
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

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

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
          isoband_cdt["iri"] = iri;
          isoband_cdt["time"] = Fmi::to_iso_extended_string(valid_time);
          isoband_cdt["parameter"] = *parameter;
          isoband_cdt["data"] = Geometry::toString(*geom2, theState.getType(), box, crs, precision);
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

          theGlobals["paths"][iri] = isoband_cdt;

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
    std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

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
    auto param = Spine::ParameterFactory::instance().parse(*parameter, allowUnknownParam);

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

      std::string report2 = "IsobandLayer::resample finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer2;
      if (theState.useTimer())
        timer2 = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report2);

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

    OGRGeometryPtr inshape, outshape;
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

    std::vector<Engine::Contour::Range> limits;
    const auto& contourer = theState.getContourEngine();
    for (const Isoband& isoband : isobands)
      limits.emplace_back(Engine::Contour::Range(isoband.lolimit, isoband.hilimit));

    Engine::Contour::Options options(param, valid_time, limits);
    options.level = level;

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    if (multiplier || offset)
      options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

    options.minarea = minarea;

    options.filter_size = smoother.size;
    options.filter_degree = smoother.degree;

    options.extrapolation = extrapolation;

    if (interpolation == "linear")
      options.interpolation = Engine::Contour::Linear;
    else if (interpolation == "nearest")
      options.interpolation = Engine::Contour::Nearest;
    else if (interpolation == "discrete")
      options.interpolation = Engine::Contour::Discrete;
    else if (interpolation == "loglinear")
      options.interpolation = Engine::Contour::LogLinear;
    else
      throw Fmi::Exception(BCP, "Unknown isoband interpolation method '" + interpolation + "'!");

    // Do the actual contouring, either full grid or just
    // a sampled section

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    boost::hash_combine(valueshash, options.data_hash_value());

    // Select the data

    if (heatmap.resolution)
    {
      // Heatmap querydata has just 1 fixed parameter (1/pressure)
      options.parameter = Spine::ParameterFactory::instance().parse("1");
    }

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

    CoordinatesPtr coords = qEngine.getWorldCoordinates(q, crs);

    std::vector<OGRGeometryPtr> geoms =
        contourer.contour(qhash, q->SpatialReference(), crs, *matrix, *coords, options);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

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
          isoband_cdt["iri"] = iri;
          isoband_cdt["time"] = Fmi::to_iso_extended_string(valid_time);
          isoband_cdt["parameter"] = *parameter;
          isoband_cdt["data"] = Geometry::toString(*geom2, theState.getType(), box, crs, precision);
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

          theGlobals["paths"][iri] = isoband_cdt;

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
    // return invalid_hash;

    auto hash = Layer::hash_value(theState);

    if (!theState.isObservation(producer) && !(source && *source == "grid"))
      Dali::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

    Dali::hash_combine(hash, Dali::hash_value(parameter));
    Dali::hash_combine(hash, Dali::hash_value(geometryId));
    Dali::hash_combine(hash, Dali::hash_value(levelId));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(forecastType));
    Dali::hash_combine(hash, Dali::hash_value(forecastNumber));
    Dali::hash_combine(hash, Dali::hash_value(isobands, theState));
    Dali::hash_combine(hash, Dali::hash_value(interpolation));
    Dali::hash_combine(hash, Dali::hash_value(smoother, theState));
    Dali::hash_combine(hash, Dali::hash_value(extrapolation));
    Dali::hash_combine(hash, Dali::hash_value(precision));
    Dali::hash_combine(hash, Dali::hash_value(minarea));
    Dali::hash_combine(hash, Dali::hash_value(unit_conversion));
    Dali::hash_combine(hash, Dali::hash_value(multiplier));
    Dali::hash_combine(hash, Dali::hash_value(offset));
    Dali::hash_combine(hash, Dali::hash_value(outside, theState));
    Dali::hash_combine(hash, Dali::hash_value(inside, theState));
    Dali::hash_combine(hash, Dali::hash_value(sampling, theState));
    Dali::hash_combine(hash, Dali::hash_value(intersections, theState));
    Dali::hash_combine(hash, Dali::hash_value(heatmap, theState));
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
