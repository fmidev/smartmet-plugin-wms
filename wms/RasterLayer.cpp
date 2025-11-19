//======================================================================

#include "RasterLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoband.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include "StyleSheet.h"
#include "ValueTools.h"
#include "ColorPainter_ARGB.h"
#include "ColorPainter_range.h"
#include "ColorPainter_shading.h"
#include "ColorPainter_stream.h"
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
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <grid-files/map/Topology.h>
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
#include <unistd.h>
#include <limits>

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
void RasterLayer::init(Json::Value &theJson, const State &theState, const Config &theConfig, const Properties &theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(parameter, theJson, "parameter");
    JsonTools::remove_string(direction, theJson, "direction");
    JsonTools::remove_string(speed, theJson, "speed");


    painter = "default";
    JsonTools::remove_string(painter, theJson, "painter");

    ColorPainter_ARGB *colorPainter_argb = new ColorPainter_ARGB();
    colorPainters.insert(std::pair<std::string,ColorPainter_sptr>("default",ColorPainter_sptr(colorPainter_argb)));

    ColorPainter_stream *colorPainter_stream = new ColorPainter_stream();
    colorPainters.insert(std::pair<std::string,ColorPainter_sptr>("stream",ColorPainter_sptr(colorPainter_stream)));

    std::string opacity_land = "1.0";
    JsonTools::remove_string(opacity_land, theJson, "opacity_land");
    painterParameters.insert(std::pair<std::string,std::string>("opacity_land",opacity_land));

    std::string opacity_sea = "1.0";
    JsonTools::remove_string(opacity_sea, theJson, "opacity_sea");
    painterParameters.insert(std::pair<std::string,std::string>("opacity_sea",opacity_sea));

    if (painter == "default")
    {

      std::string colormap;
      JsonTools::remove_string(colormap, theJson, "colormap");
      painterParameters.insert(std::pair<std::string,std::string>("colormap",colormap));

      std::string smooth;
      JsonTools::remove_string(smooth, theJson, "smooth");
      painterParameters.insert(std::pair<std::string,std::string>("smooth",smooth));

      if (!colormap.empty())
      {
        std::string cmap = theState.getColorMap(colormap);
        if (!cmap.empty())
        {
          colorPainter_argb->addColorMap(colormap,cmap);
        }
        else
        {
          Fmi::Exception exception(BCP, "Cannot find the colormap!");
          exception.addParameter("colormap",colormap);
          throw exception;
        }
      }
    }

    if (painter == "range")
    {
      std::string min_value = "0.0";
      JsonTools::remove_string(min_value, theJson, "min_value");
      painterParameters.insert(std::pair<std::string,std::string>("min_value",min_value));

      std::string max_value = "1.0";
      JsonTools::remove_string(max_value, theJson, "max_value");
      painterParameters.insert(std::pair<std::string,std::string>("max_value",max_value));

      std::string min_color = "00FFFF00";
      JsonTools::remove_string(min_color, theJson, "min_color");
      painterParameters.insert(std::pair<std::string,std::string>("min_color",min_color));

      std::string max_color = "FFFFFF00";
      JsonTools::remove_string(max_color, theJson, "max_color");
      painterParameters.insert(std::pair<std::string,std::string>("max_color",max_color));

      std::string low_color = "00000000";
      JsonTools::remove_string(low_color, theJson, "low_color");
      painterParameters.insert(std::pair<std::string,std::string>("low_color",low_color));

      std::string high_color = "00000000";
      JsonTools::remove_string(high_color, theJson, "high_color");
      painterParameters.insert(std::pair<std::string,std::string>("high_color",high_color));
    }


    if (painter == "stream")
    {
      std::string stream_color = "00FFFFFF";
      JsonTools::remove_string(stream_color, theJson, "stream_color");
      painterParameters.insert(std::pair<std::string,std::string>("stream_color",stream_color));

      std::string stream_min_length = "10";
      JsonTools::remove_string(stream_min_length, theJson, "stream_min_length");
      painterParameters.insert(std::pair<std::string,std::string>("stream_min_length",stream_min_length));

      std::string stream_max_length = "10";
      JsonTools::remove_string(stream_max_length, theJson, "stream_max_length");
      painterParameters.insert(std::pair<std::string,std::string>("stream_max_length",stream_max_length));

      std::string stream_step_x = "10";
      JsonTools::remove_string(stream_step_x, theJson, "stream_step_x");
      painterParameters.insert(std::pair<std::string,std::string>("stream_step_x",stream_step_x));

      std::string stream_step_y = "10";
      JsonTools::remove_string(stream_step_y, theJson, "stream_step_y");
      painterParameters.insert(std::pair<std::string,std::string>("stream_step_y",stream_step_y));

      std::string colormap;
      JsonTools::remove_string(colormap, theJson, "colormap");
      painterParameters.insert(std::pair<std::string,std::string>("colormap",colormap));

      std::string smooth;
      JsonTools::remove_string(smooth, theJson, "smooth");
      painterParameters.insert(std::pair<std::string,std::string>("smooth",smooth));

      if (!colormap.empty())
      {
        std::string cmap = theState.getColorMap(colormap);
        if (!cmap.empty())
        {
          colorPainter_stream->addColorMap(colormap,cmap);
        }
        else
        {
          Fmi::Exception exception(BCP, "Cannot find the colormap!");
          exception.addParameter("colormap",colormap);
          throw exception;
        }
      }
    }


    land_position = "none";
    JsonTools::remove_string(land_position, theJson, "land_position");

    JsonTools::remove_string(land_color, theJson, "land_color");
    //landShading_parameters.insert(std::pair<std::string,std::string>("shading_background",landShading_background));


    landShading_position = "none";
    JsonTools::remove_string(landShading_position, theJson, "landShading_position");

    std::string landShading_light = "128";
    JsonTools::remove_string(landShading_light, theJson, "landShading_light");
    landShading_parameters.insert(std::pair<std::string,std::string>("shading_light",landShading_light));

    std::string landShading_shadow = "384";
    JsonTools::remove_string(landShading_shadow, theJson, "landShading_shadow");
    landShading_parameters.insert(std::pair<std::string,std::string>("shading_shadow",landShading_shadow));



    sea_position = "none";
    JsonTools::remove_string(sea_position, theJson, "sea_position");

    JsonTools::remove_string(sea_color, theJson, "sea_color");
    //seaShading_parameters.insert(std::pair<std::string,std::string>("shading_background",seaShading_background));

    seaShading_position = "none";
    JsonTools::remove_string(seaShading_position, theJson, "seaShading_position");

    std::string seaShading_light = "128";
    JsonTools::remove_string(seaShading_light, theJson, "seaShading_light");
    seaShading_parameters.insert(std::pair<std::string,std::string>("shading_light",seaShading_light));

    std::string seaShading_shadow = "384";
    JsonTools::remove_string(seaShading_shadow, theJson, "seaShading_shadow");
    seaShading_parameters.insert(std::pair<std::string,std::string>("shading_shadow",seaShading_shadow));


    compression = 1;
    JsonTools::remove_int(compression, theJson, "compression");

    colorPainters.insert(std::pair<std::string,ColorPainter_sptr>("range",ColorPainter_sptr(new ColorPainter_range())));

    JsonTools::remove_string(interpolation, theJson, "interpolation");
    JsonTools::remove_string(unit_conversion, theJson, "unit_conversion");
    JsonTools::remove_double(multiplier, theJson, "multiplier");
    JsonTools::remove_double(offset, theJson, "offset");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



void RasterLayer::generate(CTPP::CDT &theGlobals, CTPP::CDT &theLayersCdt, State &theState)
{
  try
  {
    if (!validLayer(theState))
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid).addParameter("Producer", *producer);
  }
}



void RasterLayer::generate_gridEngine(CTPP::CDT &theGlobals, CTPP::CDT &theLayersCdt, State &theState)
{
  try
  {
    const auto *gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    if (!parameter && !(direction && speed))
      throw Fmi::Exception(BCP, "Parameter not set for raster-layer");

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "RasterLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);

    // Establish the valid time
    auto valid_time = getValidTime();

    T::ParamValue_vec contourLowValues;
    T::ParamValue_vec contourHighValues;
    for (const Isoband &isoband : isobands)
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

    auto crs = projection.getCRS();
    const auto &box = projection.getBox();
    //const auto clipbox = getClipBox(box);

    std::string wkt = *projection.crs;

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
        wkt = crs.WKT();

      // std::cout << wkt << "\n";

      // Adding the bounding box information into the query.

      //auto bbox = fmt::format("{},{},{},{}", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(), clipbox.ymax());
      auto bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();

      // # Testing if the target grid is defined as latlon:
      if (projection.x1 == bl.X() && projection.y1 == bl.Y() && projection.x2 == tr.X() && projection.y2 == tr.Y())
        originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
      //originalGridQuery->mAttributeList.addAttribute("grid.countSize", "1");
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
      std::string pName = *parameter;
      auto pos = pName.find(".raw");
      if (pos != std::string::npos)
      {
        attributeList.addAttribute("grid.areaInterpolationMethod", Fmi::to_string(T::AreaInterpolationMethod::Nearest));
        pName.erase(pos, 4);
      }

      std::string param = gridEngine->getParameterString(producerName, pName);

      if (multiplier && *multiplier != 1.0)
        param = "MUL{" + param + ";" + std::to_string(*multiplier) + "}";

      if (offset && *offset)
        param = "SUM{" + param + ";" + std::to_string(*offset) + "}";

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
    else
    if (direction && speed)
    {
      std::string dir = gridEngine->getParameterString(producerName, *direction);
      std::string sp = gridEngine->getParameterString(producerName, *speed);

      attributeList.addAttribute("param", dir + "," + sp);

      if (!projection.projectionParameter)
        projection.projectionParameter = dir;

      if (dir == *direction && originalGridQuery->mProducerNameList.empty())
      {
        gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
        if (originalGridQuery->mProducerNameList.empty())
          originalGridQuery->mProducerNameList.push_back(producerName);
      }
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

    for (auto &p : originalGridQuery->mQueryParameterList)
    {
      p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      p.mType = QueryServer::QueryParameter::Type::Vector;
      p.mFlags |= QueryServer::QueryParameter::Flags::ReturnCoordinates;

      if (interpolation == "nearest")
        p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Nearest;
      else
      if (interpolation == "linear")
        p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Linear;

      if (geometryId)
        p.mGeometryId = *geometryId;

      if (levelId)
        p.mParameterLevelId = *levelId;

      if (level)
      {
        p.mParameterLevel = C_INT(*level);
      }
      else if (pressure)
      {
        p.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
        p.mParameterLevel = C_INT(*pressure);
      }

      if (elevation_unit)
      {
        if (*elevation_unit == "m")
          p.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;

        if (*elevation_unit == "p")
          p.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
      }

      if (forecastType)
        p.mForecastType = C_INT(*forecastType);

      if (forecastNumber)
        p.mForecastNumber = C_INT(*forecastNumber);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    if (projection.size && *projection.size > 0)
    {
      originalGridQuery->mAttributeList.addAttribute("grid.size", Fmi::to_string(*projection.size));
    }
    else
    {
      if (projection.xsize)
        originalGridQuery->mAttributeList.addAttribute("grid.width", Fmi::to_string(*projection.xsize));

      if (projection.ysize)
        originalGridQuery->mAttributeList.addAttribute("grid.height", Fmi::to_string(*projection.ysize));
    }

    if (projection.bboxcrs)
      originalGridQuery->mAttributeList.addAttribute("grid.bboxcrs", *projection.bboxcrs);

    if (projection.cx)
      originalGridQuery->mAttributeList.addAttribute("grid.cx", Fmi::to_string(*projection.cx));

    if (projection.cy)
      originalGridQuery->mAttributeList.addAttribute("grid.cy", Fmi::to_string(*projection.cy));

    if (projection.resolution)
      originalGridQuery->mAttributeList.addAttribute("grid.resolution", Fmi::to_string(*projection.resolution));

    if (wkt == "data" && projection.x1 && projection.y1 && projection.x2 && projection.y2)
    {
      auto bbox = fmt::format("{},{},{},{}", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    //query->print(std::cout, 0, 0);



    // Extracting the projection information from the query result.

    if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
    {
      const char *widthStr = query->mAttributeList.getAttributeValue("grid.width");
      const char *heightStr = query->mAttributeList.getAttributeValue("grid.height");

      if (widthStr != nullptr)
        projection.xsize = Fmi::stoi(widthStr);

      if (heightStr != nullptr)
        projection.ysize = Fmi::stoi(heightStr);
    }

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    if (*projection.crs == "data")
    {
      const char *crsStr = query->mAttributeList.getAttributeValue("grid.crs");
      const char *proj4Str = query->mAttributeList.getAttributeValue("grid.proj4");
      if (proj4Str != nullptr && strstr(proj4Str, "+lon_wrap") != nullptr)
        crsStr = proj4Str;

      if (crsStr != nullptr)
      {
        projection.crs = crsStr;
        if (!projection.bboxcrs)
        {
          const char *bboxStr = query->mAttributeList.getAttributeValue("grid.bbox");
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

    if (wkt == "data")
      return;

    int width = *projection.xsize;
    int height = *projection.ysize;

    uint sz = width * height;
    uint *image = new uint[sz];
    memset(image,0x00,sz*4);

    std::ostringstream svgImage;


    const T::Coordinate_vec *coordinates = nullptr;
    std::shared_ptr<SmartMet::QueryServer::ParameterValues> p[2];
    ColorPainter_range rangePainter;

    uint c = 0;

    auto painterRec = colorPainters.find(painter);
    if (painterRec != colorPainters.end())
    {
      auto painter = painterRec->second;
      for (const auto &query_param : query->mQueryParameterList)
      {
        for (const auto &val : query_param.mValueList)
        {
          if (!val->mValueVector.empty()  &&  c < 2)
          {
            p[c] = val;
            if (query_param.mCoordinates.size())
              coordinates = &query_param.mCoordinates;
            c++;
          }
        }
      }

      int rotate = 0;
      if (coordinates && coordinates->size() > C_UINT(10*width)  &&  (*coordinates)[0].y() < (*coordinates)[10*width].y())
        rotate = 1;


      std::vector<float> landShadings;
      std::vector<float> seaShadings;
      std::vector<float> land;

      uint sea_backcol = 0;
      if (!sea_color.empty())
        sea_backcol = strtoul(sea_color.c_str(),nullptr,16);

      uint land_backcol = 0;
      if (!land_color.empty())
        land_backcol = strtoul(land_color.c_str(),nullptr,16);

      if (coordinates)
      {
        SmartMet::Map::topology.getLand(*coordinates,land);

        if (sea_position == "bottom" &&  sea_backcol)
        {
          Parameters parameters;
          parameters.insert(std::pair<std::string,std::string>("min_value",std::to_string(-0.1)));
          parameters.insert(std::pair<std::string,std::string>("min_color",sea_color));
          parameters.insert(std::pair<std::string,std::string>("max_value",std::to_string(0.1)));
          parameters.insert(std::pair<std::string,std::string>("max_color",sea_color));
          rangePainter.setImageColors(width,height,image,land,land,parameters);
        }

        if (land_position == "bottom" &&  land_backcol)
        {
          Parameters parameters;
          parameters.insert(std::pair<std::string,std::string>("min_value",std::to_string(0.9)));
          parameters.insert(std::pair<std::string,std::string>("min_color",land_color));
          parameters.insert(std::pair<std::string,std::string>("max_value",std::to_string(1.1)));
          parameters.insert(std::pair<std::string,std::string>("max_color",land_color));

          rangePainter.setImageColors(width,height,image,land,land,parameters);
        }

        if (seaShading_position == "bottom")
        {
          SmartMet::Map::topology.getSeaShading(*coordinates,seaShadings);
          seaShading_parameters.insert(std::pair<std::string,std::string>("rotate",std::to_string(rotate)));
          shadingPainter.setImageColors(width,height,image,land,seaShadings,seaShading_parameters);
        }

        if (landShading_position == "bottom")
        {
          SmartMet::Map::topology.getLandShading(*coordinates,landShadings);
          landShading_parameters.insert(std::pair<std::string,std::string>("rotate",std::to_string(rotate)));
          shadingPainter.setImageColors(width,height,image,land,landShadings,landShading_parameters);
        }
      }

      if (c == 1)
        painter->setImageColors(width,height,image,land,p[0]->mValueVector,painterParameters);
      else
      if (c == 2)
        painter->setImageColors(width,height,image,land,p[0]->mValueVector,p[1]->mValueVector,painterParameters);

      if (coordinates)
      {
        if (sea_position == "top" &&  sea_backcol)
        {
          Parameters parameters;
          parameters.insert(std::pair<std::string,std::string>("min_value",std::to_string(-0.1)));
          parameters.insert(std::pair<std::string,std::string>("min_color",sea_color));
          parameters.insert(std::pair<std::string,std::string>("max_value",std::to_string(0.1)));
          parameters.insert(std::pair<std::string,std::string>("max_color",sea_color));
          rangePainter.setImageColors(width,height,image,land,land,parameters);
        }

        if (land_position == "top" &&  land_backcol)
        {
          Parameters parameters;
          parameters.insert(std::pair<std::string,std::string>("min_value",std::to_string(0.9)));
          parameters.insert(std::pair<std::string,std::string>("min_color",land_color));
          parameters.insert(std::pair<std::string,std::string>("max_value",std::to_string(1.1)));
          parameters.insert(std::pair<std::string,std::string>("max_color",land_color));

          rangePainter.setImageColors(width,height,image,land,land,parameters);
        }

        if (seaShading_position == "top")
        {
          if (seaShadings.size() == 0)
             SmartMet::Map::topology.getSeaShading(*coordinates,seaShadings);

          seaShading_parameters.insert(std::pair<std::string,std::string>("shading_position","top"));
          seaShading_parameters.insert(std::pair<std::string,std::string>("rotate",std::to_string(rotate)));
          shadingPainter.setImageColors(width,height,image,land,seaShadings,seaShading_parameters);
        }

        if (landShading_position == "top")
        {
          if (landShadings.size() == 0)
            SmartMet::Map::topology.getLandShading(*coordinates,landShadings);

          landShading_parameters.insert(std::pair<std::string,std::string>("shading_position","top"));
          landShading_parameters.insert(std::pair<std::string,std::string>("rotate",std::to_string(rotate)));
          shadingPainter.setImageColors(width,height,image,land,landShadings,landShading_parameters);
        }
      }

      int bsz = sz*4 + 10000;
      char *buffer = new char[bsz];
      int nsz = png_saveMem(buffer,bsz,image,width,height,compression);

      svgImage << "<image id=\"" << qid << "\" href=\"data:image/png;base64,";
      svgImage << base64_encode((unsigned char*)buffer,nsz);
      svgImage << "\" x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << height << "\" />\n\n";

      delete [] buffer;
    }

    if (image)
    {
      delete [] image;
      image = NULL;
    }

    CTPP::CDT object_cdt;
    std::string objectKey;

    if (parameter)
      objectKey = "raster:" + *parameter + ":" + qid;

    object_cdt["objectKey"] = objectKey;

    // Clip if necessary
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate streamlines as use tags statements inside <g>..</g>
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    std::ostringstream useOut;
    useOut << "<use xlink:href=\"#" << qid << "\"/>\n";

    theGlobals["includes"][qid] = svgImage.str();
    theGlobals["bbox"] = Fmi::to_string(box.xmin()) + "," + Fmi::to_string(box.ymin()) + "," +
                         Fmi::to_string(box.xmax()) + "," + Fmi::to_string(box.ymax());
    theGlobals["objects"][objectKey] = object_cdt;

    // We created only this one layer
    group_cdt["cdata"] = useOut.str();
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void RasterLayer::generate_qEngine(CTPP::CDT &theGlobals, CTPP::CDT &theLayersCdt, State &theState)
{
  try
  {
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
void RasterLayer::addGridParameterInfo(ParameterInfos &infos, const State &theState) const
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
std::size_t RasterLayer::hash_value(const State &theState) const
{
  try
  {
    // if (source && *source == "grid")
    // return Fmi::bad_hash;

    auto hash = Layer::hash_value(theState);

    if (!theState.isObservation(producer) && !(source && *source == "grid"))
      Fmi::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

    Fmi::hash_combine(hash, countParameterHash(theState, parameter));
    Fmi::hash_combine(hash, Dali::hash_value(isobands, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(interpolation));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Fmi::hash_value(painter));
    Fmi::hash_combine(hash, Fmi::hash_value(compression));
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
