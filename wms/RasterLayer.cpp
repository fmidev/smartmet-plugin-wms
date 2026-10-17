//======================================================================

#include "RasterLayer.h"
#include "Base64.h"
#include "ColorPainter_ARGB.h"
#include "ColorPainter_border.h"
#include "ColorPainter_rain.h"
#include "ColorPainter_range.h"
#include "ColorPainter_shading.h"
#include "ColorPainter_stream.h"
#include "Config.h"
#include "DataTile.h"
#include "Geometry.h"
#include "GridDataGeoTiff.h"
#include "GridQueryTools.h"
#include "Hash.h"
#include "Isoband.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include "StyleSheet.h"
#include "ValueTools.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/observation/Settings.h>
#include <engines/querydata/Model.h>
#include <engines/querydata/Q.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateMatrix.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <giza/Giza.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <grid-files/grid/Typedefs.h>
#include <grid-files/map/Topography.h>
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
#include <atomic>
#include <cpl_vsi.h>
#include <gdal_priv.h>
#include <limits>
#include <ogr_spatialref.h>
#include <unistd.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// Sample one parameter onto the output grid. Values are stored bottom-up to
// match the coordinates, and the newbase missing sentinel is mapped to
// the grid-files sentinel the painters test against.
std::vector<float> sample_values(const Engine::Querydata::Q &q,
                                 const std::string &name,
                                 const Fmi::CoordinateMatrix &llmatrix,
                                 const NFmiMetTime &met_time,
                                 int width,
                                 int height)
{
  const std::size_t sz = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  auto param = TS::ParameterFactory::instance().parse(name);
  if (!q->param(param.number()))
    throw Fmi::Exception(
        BCP, "Parameter '" + name + "' is not available in the raster-layer querydata!");

  auto matrix = q->values(llmatrix, met_time);
  if (matrix.NX() != static_cast<unsigned int>(width) ||
      matrix.NY() != static_cast<unsigned int>(height))
    return {};

  std::vector<float> values(sz);
  for (int j = 0; j < height; j++)
    for (int i = 0; i < width; i++)
    {
      const float v = matrix[i][j];
      values[static_cast<std::size_t>(j) * width + i] =
          (v == kFloatMissing ? static_cast<float>(ParamValueMissing) : v);
    }
  return values;
}

// Set the interpolation methods of a raster query parameter
void set_raster_interpolation(QueryServer::QueryParameter &p, const std::string &interpolation)
{
  if (interpolation == "nearest")
    p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Nearest;
  else if (interpolation == "linear")
  {
    p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Linear;
    p.mTimeInterpolationMethod = T::TimeInterpolationMethod::Linear;
    p.mLevelInterpolationMethod = T::LevelInterpolationMethod::Linear;
  }

  if (interpolation == "transfer")
  {
    p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Linear;
    p.mTimeInterpolationMethod = 15;
    p.mLevelInterpolationMethod = T::LevelInterpolationMethod::Linear;
  }
}

// Add the direction and speed parameters to the query
void add_direction_speed_parameters(const Engine::Grid::Engine &gridEngine,
                                    const std::string &producerName,
                                    const std::string &direction,
                                    const std::string &speed,
                                    Projection &projection,
                                    T::AttributeList &attributeList,
                                    QueryServer::Query &query)
{
  std::string dir = gridEngine.getParameterString(producerName, direction);
  std::string sp = gridEngine.getParameterString(producerName, speed);

  attributeList.addAttribute("param", dir + "," + sp);

  if (!projection.projectionParameter)
    projection.projectionParameter = dir;

  if (dir == direction && query.mProducerNameList.empty())
  {
    gridEngine.getProducerNameList(producerName, query.mProducerNameList);
    if (query.mProducerNameList.empty())
      query.mProducerNameList.push_back(producerName);
  }
}

// The first two non-empty value vectors and the coordinates of the last of them
void first_two_value_vectors(const QueryServer::Query &query,
                             std::shared_ptr<SmartMet::QueryServer::ParameterValues> (&p)[2],
                             const T::Coordinate_vec *&coordinates)
{
  uint c = 0;
  for (const auto &query_param : query.mQueryParameterList)
  {
    for (const auto &val : query_param.mValueList)
    {
      if (!val->mValueVector.empty() && c < 2)
      {
        p[c] = val;
        if (!query_param.mCoordinates.empty())
          coordinates = &query_param.mCoordinates;
        c++;
      }
    }
  }
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------
void RasterLayer::init(Json::Value &theJson,
                       const State &theState,
                       const Config &theConfig,
                       const Properties &theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(direction, theJson, "direction");
    JsonTools::remove_string(speed, theJson, "speed");

    painter = "default";
    JsonTools::remove_string(painter, theJson, "data_painter");

    if (painter == "null")
    {
      dataPainter = std::make_shared<ColorPainter>();
      dataPainter->init(theJson, theState);
    }
    else if (painter == "default")
    {
      dataPainter = std::make_shared<ColorPainter_ARGB>();
      dataPainter->init(theJson, theState);
    }
    else if (painter == "range")
    {
      dataPainter = std::make_shared<ColorPainter_range>();
      dataPainter->init(theJson, theState);
    }
    else if (painter == "stream")
    {
      dataPainter = std::make_shared<ColorPainter_stream>();
      dataPainter->init(theJson, theState);
    }
    else if (painter == "rain")
    {
      dataPainter = std::make_shared<ColorPainter_rain>();
      dataPainter->init(theJson, theState);
    }
    else if (painter == "border")
    {
      dataPainter = std::make_shared<ColorPainter_border>();
      dataPainter->init(theJson, theState);
    }
    else if (painter == "shadow")
    {
      dataPainter = std::make_shared<ColorPainter_shadow>();
      dataPainter->init(theJson, theState);
    }

    // **** LAND PAINTING PARAMETERS

    land_position = "none";
    JsonTools::remove_string(land_position, theJson, "land_position");

    std::string land_color = "00000000";
    JsonTools::remove_string(land_color, theJson, "land_color");

    ColorPainter_range::Range range;
    range.value_min = 0.9;
    range.value_max = 1.1;
    range.color_min = argb(land_color);
    range.color_max = range.color_min;
    landPainter.addRange(range);

    // **** SEA PAINTING PARAMETERS

    sea_position = "none";
    JsonTools::remove_string(sea_position, theJson, "sea_position");

    std::string sea_color = "00000000";
    JsonTools::remove_string(sea_color, theJson, "sea_color");

    range.value_min = -0.1;
    range.value_max = 0.1;
    range.color_min = argb(sea_color);
    range.color_max = range.color_min;
    seaPainter.addRange(range);

    // **** LAND/SEA BORDER PARAMETERS

    land_border_position = "none";
    JsonTools::remove_string(land_border_position, theJson, "land_border_position");

    std::string land_border_color = "FF000000";
    JsonTools::remove_string(land_border_color, theJson, "land_border_color");

    std::string sea_border_color = "FFFFFFFF";
    JsonTools::remove_string(sea_border_color, theJson, "sea_border_color");

    ColorPainter_border::Border border;
    border.inside_value_min = 0.9;
    border.inside_color = argb(land_border_color);
    border.outside_color = argb(sea_border_color);
    landBorderPainter.addBorder(border);

    // **** LAND SHADING PARAMETERS

    land_shading_position = "none";
    JsonTools::remove_string(land_shading_position, theJson, "land_shading_position");

    std::string land_shading_light = "128";
    JsonTools::remove_string(land_shading_light, theJson, "land_shading_light");
    land_shading_parameters.insert(
        std::pair<std::string, std::string>("shading_light", land_shading_light));

    std::string land_shading_shadow = "384";
    JsonTools::remove_string(land_shading_shadow, theJson, "land_shading_shadow");
    land_shading_parameters.insert(
        std::pair<std::string, std::string>("shading_shadow", land_shading_shadow));

    // **** SEA SHADING PARAMETERS

    sea_shading_position = "none";
    JsonTools::remove_string(sea_shading_position, theJson, "sea_shading_position");

    std::string sea_shading_light = "128";
    JsonTools::remove_string(sea_shading_light, theJson, "sea_shading_light");
    sea_shading_parameters.insert(
        std::pair<std::string, std::string>("shading_light", sea_shading_light));

    std::string sea_shading_shadow = "384";
    JsonTools::remove_string(sea_shading_shadow, theJson, "sea_shading_shadow");
    sea_shading_parameters.insert(
        std::pair<std::string, std::string>("shading_shadow", sea_shading_shadow));

    // **** DATA BORDER PARAMETERS

    auto json = JsonTools::remove(theJson, "data_borders");
    if (!json.isNull())
      dataBorderPainter.initBorders(json, theState);

    // **** DATA SHADOW PARAMETERS

    json = JsonTools::remove(theJson, "data_shadows");
    if (!json.isNull())
      dataShadowPainter.initShadows(json, theState);

    compression = 1;
    JsonTools::remove_int(compression, theJson, "compression");

    JsonTools::remove_string(interpolation, theJson, "interpolation");
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

    if (!isNewImageRequired(theState))
    {
      // We do not need a new image so there is no need to fetch data
      std::vector<float> emptyVector;
      generate_output(theGlobals, theLayersCdt, theState, nullptr, emptyVector, emptyVector);
      return;
    }

    if (paraminfo.source == std::string("grid"))
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!")
        .addParameter("qid", qid)
        .addParameter("Producer", *paraminfo.producer);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Paint the sea, land, topography and land sea border layers at the given position
 */
// ----------------------------------------------------------------------

void RasterLayer::paintSurface(const std::string &position, PaintContext &ctx)
{
  // Painting sea
  if (sea_position == position && !ctx.land.empty())
    seaPainter.setImageColors(ctx.width,
                              ctx.height,
                              ctx.loopstep,
                              ctx.loopsteps,
                              ctx.image,
                              ctx.land,
                              ctx.land,
                              ctx.seaParameters);

  // Painting land
  if (land_position == position && !ctx.land.empty())
    landPainter.setImageColors(ctx.width,
                               ctx.height,
                               ctx.loopstep,
                               ctx.loopsteps,
                               ctx.image,
                               ctx.land,
                               ctx.land,
                               ctx.landParameters);

  // Painting sea topography
  if (sea_shading_position == position && ctx.coordinates)
  {
    if (ctx.sea_shadings.empty())
      SmartMet::Map::topography.getSeaShading(*ctx.coordinates, ctx.sea_shadings);

    shadingPainter.setImageColors(ctx.width,
                                  ctx.height,
                                  ctx.loopstep,
                                  ctx.loopsteps,
                                  ctx.image,
                                  ctx.land,
                                  ctx.sea_shadings,
                                  sea_shading_parameters);
  }

  // Painting land topography
  if (land_shading_position == position && ctx.coordinates)
  {
    if (ctx.land_shadings.empty())
      SmartMet::Map::topography.getLandShading(*ctx.coordinates, ctx.land_shadings);

    shadingPainter.setImageColors(ctx.width,
                                  ctx.height,
                                  ctx.loopstep,
                                  ctx.loopsteps,
                                  ctx.image,
                                  ctx.land,
                                  ctx.land_shadings,
                                  land_shading_parameters);
  }

  // Painting land sea border
  if (land_border_position == position && !ctx.land.empty())
    landBorderPainter.setImageColors(ctx.width,
                                     ctx.height,
                                     ctx.loopstep,
                                     ctx.loopsteps,
                                     ctx.image,
                                     ctx.land,
                                     ctx.land,
                                     ctx.landBorderParameters);
}

void RasterLayer::generate_image(int loopstep,
                                 int loopsteps,
                                 const T::Coordinate_vec *coordinates,
                                 std::vector<float> &values1,
                                 std::vector<float> &values2,
                                 CImage &cimage)
{
  try
  {
    // *********************  DATA SOURCE INDEPENDNT PAINTING *********

    if (values1.empty())
      return;

    int width = *projection.xsize;
    int height = *projection.ysize;
    uint sz = width * height;

    // If the animation is not enabled on this layer, then we should return
    // the same image in all loopsteps.

    if (!animation_enabled)
      loopstep = 0;

    int rotate = 0;
    if (coordinates && coordinates->size() > C_UINT(10 * width) &&
        (*coordinates)[0].y() < (*coordinates)[10 * width].y())
      rotate = 1;

    std::vector<float> land_shadings;
    std::vector<float> sea_shadings;
    std::vector<float> land;

    Parameters seaParameters;
    Parameters landParameters;
    Parameters landBorderParameters;

    if (coordinates)
    {
      SmartMet::Map::topography.getLand(*coordinates, land);

      sea_shading_parameters.insert(
          std::pair<std::string, std::string>("rotate", std::to_string(rotate)));
      land_shading_parameters.insert(
          std::pair<std::string, std::string>("rotate", std::to_string(rotate)));
    }

    uint *image = new uint[sz];
    memset(image, 0x00, sz * 4);

    cimage.width = width;
    cimage.height = height;
    cimage.pixel = image;
    Parameters emptyParameters;

    PaintContext ctx{width,
                     height,
                     loopstep,
                     loopsteps,
                     image,
                     coordinates,
                     land,
                     sea_shadings,
                     land_shadings,
                     seaParameters,
                     landParameters,
                     landBorderParameters};

    // Painting the background layers below the data
    paintSurface("bottom", ctx);

    if (dataShadowPainter.getShadowCount())
      dataShadowPainter.setImageColors(
          width, height, loopstep, loopsteps, image, land, values1, emptyParameters);

    // Painting the actual parameter
    if (dataPainter)
    {
      if (values2.empty())
        dataPainter->setImageColors(
            width, height, loopstep, loopsteps, image, land, values1, painterParameters);
      else
        dataPainter->setImageColors(
            width, height, loopstep, loopsteps, image, land, values1, values2, painterParameters);
    }

    if (dataBorderPainter.getBorderCount())
      dataBorderPainter.setImageColors(
          width, height, loopstep, loopsteps, image, land, values1, emptyParameters);

    // Painting the background layers on top of the data
    paintSurface("top", ctx);

    std::ostringstream svgImage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool RasterLayer::isNewImageRequired(const State& theState)
{
  try
  {
    if (!theState.animation_enabled)
      return true;

    if (svg_image.empty())
      return true;

    if (dataPainter)
    {
      if (dataPainter->isAnimator())
        return true;

      if (theState.animation_loopstep == 0)
        return true;
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void RasterLayer::generate_output(CTPP::CDT &theGlobals,
                                  CTPP::CDT &theLayersCdt,
                                  const State& theState,
                                  const T::Coordinate_vec *coordinates,
                                  std::vector<float> &values1,
                                  std::vector<float> &values2)
{
  try
  {
    // *********************  DATA SOURCE INDEPENDENT PAINTING *********

    bool vis = true;
    if (theState.animation_enabled)
    {
      vis = isAnimationStepVisible(theState);
      if (!vis)
        svg_image = "";
    }

    if (isNewImageRequired(theState) && visible && vis)
    {
      CImage cimage;
      generate_image(theState.animation_loopstep,
                     theState.animation_loopsteps,
                     coordinates,
                     values1,
                     values2,
                     cimage);

      std::ostringstream svgImage;

      int comp = compression;
      if (theState.animation_enabled)
        comp = 1;

      const auto png = Giza::topng_argb(cimage.pixel, cimage.width, cimage.height, comp);
      svgImage << "<image id=\"" << Fmi::safexmlescape(qid) << "\" xlink:href=\"data:image/png;base64,";
      svgImage << Dali::base64_encode(png);
      svgImage << R"(" x="0" y="0" width=")" << cimage.width << "\" height=\"" << cimage.height
               << "\" />\n\n";

      svg_image = svgImage.str();
    }

    CTPP::CDT object_cdt;
    std::string objectKey;
    const auto &box = projection.getBox();

    if (!paraminfo.parameter.empty())
      objectKey = "raster:" + paraminfo.parameter + ":" + qid;

    object_cdt["objectKey"] = objectKey;

    // Clip if necessary
    // addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate streamlines as use tags statements inside <g>..</g>
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    std::ostringstream useOut;
    useOut << "<use xlink:href=\"#" << Fmi::safexmlescape(qid) << "\"/>\n";

    if (visible && !svg_image.empty())
      theGlobals["includes"][qid] = svg_image;

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

void RasterLayer::generate_gridEngine(CTPP::CDT &theGlobals,
                                      CTPP::CDT &theLayersCdt,
                                      const State& theState)
{
  try
  {
    const auto *gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    if (paraminfo.parameter.empty() && !(direction && speed))
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

    std::string producerName = gridEngine->getProducerName(*paraminfo.producer);

    // Establish the valid time
    auto valid_time = getValidTime();

    auto crs = projection.getCRS();
    const auto &box = projection.getBox();
    std::string wkt = *projection.crs;

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
        wkt = crs.WKT();

      // std::cout << wkt << "\n";

      // Adding the bounding box information into the query.

      // auto bbox = fmt::format("{},{},{},{}", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(),
      // clipbox.ymax());
      auto bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();

      // # Testing if the target grid is defined as latlon:
      if (projection.x1 == bl.X() && projection.y1 == bl.Y() && projection.x2 == tr.X() &&
          projection.y2 == tr.Y())
        originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means
      // that we we do not know the actual projection yet and we have to wait that the grid-engine
      // delivers us the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    if (!paraminfo.parameter.empty())
    {
      add_grid_parameter(*gridEngine,
                         producerName,
                         paraminfo.parameter,
                         multiplier,
                         offset,
                         projection,
                         attributeList,
                         *originalGridQuery);
    }
    else if (direction && speed)
    {
      add_direction_speed_parameters(*gridEngine,
                                     producerName,
                                     *direction,
                                     *speed,
                                     projection,
                                     attributeList,
                                     *originalGridQuery);
    }

    add_time_attributes(attributeList, getValidTime(), origintime);

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery, attributeList);

    // Fullfilling information into the query object.

    for (auto &p : originalGridQuery->mQueryParameterList)
    {
      p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      p.mType = QueryServer::QueryParameter::Type::Vector;
      p.mFlags |= QueryServer::QueryParameter::Flags::ReturnCoordinates;

      set_raster_interpolation(p, interpolation);

      set_query_parameter_selections(p, paraminfo);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    add_grid_size_attributes(*originalGridQuery, projection);

    add_projection_attributes(*originalGridQuery, projection);

    if (wkt == "data")
      add_data_bbox_attribute(*originalGridQuery, projection);

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    // query->print(std::cout, 0, 0);

    // Extracting the projection information from the query result.

    update_projection_size(projection, *query);

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    update_data_projection(projection, *query);

    if (wkt == "data")
      return;

    const T::Coordinate_vec *coordinates = nullptr;
    std::shared_ptr<SmartMet::QueryServer::ParameterValues> p[2];

    first_two_value_vectors(*query, p, coordinates);

    // *********************  DATA SOURCE INDEPENDNT PAINTING *********

    if (p[0] && !p[1])
    {
      std::vector<float> emptyVector;
      generate_output(
          theGlobals, theLayersCdt, theState, coordinates, p[0]->mValueVector, emptyVector);
    }
    else if (p[0] && p[1])
      generate_output(
          theGlobals, theLayersCdt, theState, coordinates, p[0]->mValueVector, p[1]->mValueVector);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void RasterLayer::generate_qEngine(CTPP::CDT& theGlobals,
                                   CTPP::CDT& theLayersCdt,
                                   const State& theState)
{
  try
  {
    // 1. Establish the querydata and the parameter(s) to render.

    auto q = getModel(theState);
    if (!q)
      throw Fmi::Exception(BCP, "Raster-layer requires querydata to be available");
    if (!q->isGrid())
      throw Fmi::Exception(BCP, "Raster-layer can't use point data!");

    const bool have_param = !paraminfo.parameter.empty();
    const bool have_wind = (direction && speed);
    if (!have_param && !have_wind)
      throw Fmi::Exception(BCP,
                           "Neither 'parameter' nor 'direction'+'speed' set for raster-layer!");

    if (!q->firstLevel())
      throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

    if (paraminfo.level)
    {
      if (!q->selectLevel(*paraminfo.level))
        throw Fmi::Exception(
            BCP, "Level value " + Fmi::to_string(*paraminfo.level) + " is not available!");
    }

    // 2. Establish the output projection grid.

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();
    const int width = *projection.xsize;
    const int height = *projection.ysize;
    const std::size_t sz = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    NFmiMetTime met_time = getValidTime();

    // 3. Coordinates of the current grid.
    //
    // The pixels are sampled as WGS84 longitude/latitude (what the topography
    // land/sea lookup expects) and stored bottom-up (row j=0 at box.ymin) to
    // match the painters, which flip vertically so image row 0 is the top.

    Fmi::CoordinateMatrix llmatrix(width, height, box.xmin(), box.ymin(), box.xmax(), box.ymax());
    static const Fmi::SpatialReference wgs84("WGS84");
    Fmi::CoordinateTransformation to_wgs84(crs, wgs84);
    llmatrix.transform(to_wgs84);

    T::Coordinate_vec coordinates;
    coordinates.reserve(sz);
    for (int j = 0; j < height; j++)
      for (int i = 0; i < width; i++)
        coordinates.emplace_back(llmatrix.x(i, j), llmatrix.y(i, j));

    // 4. Fetch the values (parameter => values1; or direction => values1,
    //    speed => values2) and paint.

    std::vector<float> values1;
    std::vector<float> values2;
    if (have_param)
      values1 = sample_values(q, paraminfo.parameter, llmatrix, met_time, width, height);
    else
    {
      values1 = sample_values(q, *direction, llmatrix, met_time, width, height);
      values2 = sample_values(q, *speed, llmatrix, met_time, width, height);
    }

    generate_output(theGlobals, theLayersCdt, theState, &coordinates, values1, values2);
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

void RasterLayer::getFeatureInfo(CTPP::CDT & /* theInfo */, const State & /* theState */)
{
  // not implemented yet
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

    if (!theState.isObservation(paraminfo.producer) && (paraminfo.source != std::string("grid")))
      Fmi::hash_combine(hash, getModelHashValueOrEmpty(theState));

    Fmi::hash_combine(hash, countParameterHash(theState, paraminfo.parameter));
    Fmi::hash_combine(hash, Fmi::hash_value(interpolation));
    Fmi::hash_combine(hash, Fmi::hash_value(land_position));
    Fmi::hash_combine(hash, Fmi::hash_value(sea_position));
    Fmi::hash_combine(hash, Fmi::hash_value(land_border_position));
    Fmi::hash_combine(hash, Fmi::hash_value(land_shading_position));
    Fmi::hash_combine(hash, Fmi::hash_value(sea_shading_position));
    Fmi::hash_combine(hash, Fmi::hash_value(painter));

    Fmi::hash_combine(hash, landPainter.hash_value(theState));
    Fmi::hash_combine(hash, seaPainter.hash_value(theState));
    Fmi::hash_combine(hash, landBorderPainter.hash_value(theState));
    Fmi::hash_combine(hash, dataShadowPainter.hash_value(theState));
    Fmi::hash_combine(hash, dataBorderPainter.hash_value(theState));

    if (dataPainter)
      Fmi::hash_combine(hash, dataPainter->hash_value(theState));

    Fmi::hash_combine(hash, Fmi::hash_value(compression));

    for (const auto& painterParameter : painterParameters)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(painterParameter.first));
      Fmi::hash_combine(hash, Fmi::hash_value(painterParameter.second));
    }

    for (const auto& land_shading_parameter : land_shading_parameters)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(land_shading_parameter.first));
      Fmi::hash_combine(hash, Fmi::hash_value(land_shading_parameter.second));
    }

    for (const auto& sea_shading_parameter : sea_shading_parameters)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(sea_shading_parameter.first));
      Fmi::hash_combine(hash, Fmi::hash_value(sea_shading_parameter.second));
    }
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch grid data and return GeoTiff bytes
 *
 * Bypasses the SVG/Cairo pipeline entirely. The query setup mirrors
 * generate_gridEngine() but stops after extracting the raw float values
 * and writes them into a GDAL GTiff via the /vsimem/ virtual filesystem.
 */
// ----------------------------------------------------------------------

std::string RasterLayer::generateGeoTiff(State& theState)
{
  try
  {
    return gridDataGeoTiff(*this, paraminfo.parameter, interpolation, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "RasterLayer::generateGeoTiff failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate datatile output for a single scalar parameter
 */
// ----------------------------------------------------------------------

std::string RasterLayer::generateDataTile(State& theState)
{
  try
  {
    return gridDataTile(*this, paraminfo.parameter, interpolation, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "RasterLayer::generateDataTile failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
