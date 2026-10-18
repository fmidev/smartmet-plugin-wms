#include "FrameLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include "TextUtility.h"
#include <cmath>
#include <ctpp2/CDT.hpp>
#include <gis/OGR.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <ogr_geometry.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
void readBoundingBox(const std::string& bbox, FrameDimension& dimension)
{
  if (bbox.find(',') == std::string::npos)
    throw Fmi::Exception(BCP,
                         "Frame layer error: invalid bounding box format: should be <lon lat, "
                         "lon lat> (bottom left, top right)");

  std::string bottomLeft = bbox.substr(0, bbox.find(','));
  std::string topRight = bbox.substr(bbox.find(',') + 1);
  boost::algorithm::trim(bottomLeft);
  boost::algorithm::trim(topRight);

  std::vector<std::string> parts;
  boost::algorithm::split(parts, bottomLeft, boost::algorithm::is_any_of(" "));
  if (parts.size() != 2)
    throw Fmi::Exception(BCP,
                         "Frame layer error: invalid bounding box format: should be <lon lat, "
                         "lon lat> (bottom left, top right)");
  dimension.leftLongitude = Fmi::stod(parts[0]);
  dimension.bottomLatitude = Fmi::stod(parts[1]);

  parts.clear();

  boost::algorithm::split(parts, topRight, boost::algorithm::is_any_of(" "));
  if (parts.size() != 2)
    throw Fmi::Exception(BCP,
                         "Frame layer error: invalid bounding box format: should be <lon lat, "
                         "lon lat> (bottom left, top right)");
  dimension.rightLongitude = Fmi::stod(parts[0]);
  dimension.topLatitude = Fmi::stod(parts[1]);
}

OGRGeometryPtr createFrameGeometry(const FrameDimension& inner, std::optional<FrameDimension> outer)
{
  OGRGeometryPtr pFrameGeom;

  auto* polygon = dynamic_cast<OGRPolygon*>(OGRGeometryFactory::createGeometry(wkbPolygon));

  if (outer)
  {
    auto* exterior =
        dynamic_cast<OGRLinearRing*>(OGRGeometryFactory::createGeometry(wkbLinearRing));

    exterior->addPoint(outer->leftLongitude, outer->bottomLatitude);
    exterior->addPoint(outer->leftLongitude, outer->topLatitude);
    exterior->addPoint(outer->rightLongitude, outer->topLatitude);
    exterior->addPoint(outer->rightLongitude, outer->bottomLatitude);
    exterior->addPoint(outer->leftLongitude, outer->bottomLatitude);
    polygon->addRingDirectly(exterior);
  }
  auto* interior = dynamic_cast<OGRLinearRing*>(OGRGeometryFactory::createGeometry(wkbLinearRing));
  interior->addPoint(inner.leftLongitude, inner.bottomLatitude);
  interior->addPoint(inner.rightLongitude, inner.bottomLatitude);
  interior->addPoint(inner.rightLongitude, inner.topLatitude);
  interior->addPoint(inner.leftLongitude, inner.topLatitude);
  interior->addPoint(inner.leftLongitude, inner.bottomLatitude);

  polygon->addRingDirectly(interior);

  pFrameGeom.reset(polygon);

  return pFrameGeom;
}
// True if the value is at a tic of the given (optional) tic type
bool at_tic(double v, const std::optional<TicInfo>& tic)
{
  return tic && std::abs(std::remainder(v, tic->step)) <= 0.01;
}

// True if the value is at a label position. No labels are drawn without a label step.
bool at_label(double v, const std::optional<double>& step)
{
  return step && std::abs(std::remainder(v, *step)) <= 0.01;
}

// Collect the long, intermediate and small tics strictly inside the given range.
// An intermediate tic is added only if there is no long tic at the same position,
// a small tic only if there is no long or intermediate tic.
std::map<double, TicInfo> collect_tics(
    const FrameScale& scale, int vMin, int vMax, double lowLimit, double highLimit)
{
  std::map<double, TicInfo> tics;
  // first add long tics
  if (scale.longTic)
  {
    double v = vMin + scale.longTic->step;
    while (v < vMax)
    {
      if (v > lowLimit && v < highLimit)
        tics.insert(std::make_pair(v, *(scale.longTic)));
      v += scale.longTic->step;
    }
  }

  if (scale.intermediateTic)
  {
    double v = vMin + scale.intermediateTic->step;
    while (v < vMax)
    {
      if (!at_tic(v, scale.longTic) && v > lowLimit && v < highLimit)
        tics.insert(std::make_pair(v, *(scale.intermediateTic)));
      v += scale.intermediateTic->step;
    }
  }

  if (scale.smallTic)
  {
    double v = vMin + scale.smallTic->step;
    while (v < vMax)
    {
      if (!at_tic(v, scale.longTic) && !at_tic(v, scale.intermediateTic) && v > lowLimit &&
          v < highLimit)
        tics.insert(std::make_pair(v, *(scale.smallTic)));
      v += scale.smallTic->step;
    }
  }
  return tics;
}

// Move the left and right latitude labels away from the tics
void latitude_label_offsets(bool labelIsOutside,
                            bool ticIsOutside,
                            double width,
                            int length,
                            double& lonStartLeft,
                            double& lonStartRight)
{
  if (labelIsOutside)
  {
    if (ticIsOutside)  // label out, tic out
    {
      lonStartLeft -= (width + (length * 1.3));
      lonStartRight += (length * 1.3);
    }
    else  // label out, tic in
    {
      lonStartLeft -= (width + (length * 0.3));
      lonStartRight += (width * +0.3);
    }
  }
  else
  {
    if (ticIsOutside)  // label in, tic out
    {
      lonStartLeft += (width * 0.3);
      lonStartRight -= (width * 1.3);
    }
    else  // label in, tic in
    {
      lonStartLeft += (length * 1.3);
      lonStartRight -= (width + (length * 1.3));
    }
  }
}

// Move the top and bottom longitude labels away from the tics
void longitude_label_offsets(bool labelIsOutside,
                             bool ticIsOutside,
                             double height,
                             int length,
                             double& latStartTop,
                             double& latStartBottom)
{
  if (labelIsOutside)
  {
    if (ticIsOutside)  // label out, tic out
    {
      latStartTop -= (length * 1.3);
      latStartBottom += (height + (length * 1.3));
    }
    else  // label out, tic in
    {
      latStartTop -= (height * 0.3);
      latStartBottom += (height * 1.3);
    }
  }
  else
  {
    if (ticIsOutside)  // label in tic out
    {
      latStartTop += (height * 1.3);
      latStartBottom -= (height * 0.3);
    }
    else  // label in tic in
    {
      latStartTop += (height + (length * 1.3));
      latStartBottom -= (length * 1.3);
    }
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------
void FrameLayer::init(Json::Value& theJson,
                      const State& theState,
                      const Config& theConfig,
                      const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Frame-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    itsPrecision = theState.getPrecision("frame");

    // Extract member values

    JsonTools::remove_double(itsPrecision, theJson, "precision");
    JsonTools::remove_string(itsPattern, theJson, "pattern");

    auto json = JsonTools::remove(theJson, "scale_attributes");
    if (!json.isNull())
      itsScaleAttributes.init(json, theConfig);

    auto jdim = JsonTools::remove(theJson, "dimension");
    if (jdim.isNull())
      throw Fmi::Exception(BCP, "Frame-layer must have dimension element!");

    json = JsonTools::remove(jdim, "inner_border");
    if (json.isNull())
      throw Fmi::Exception(BCP, "Frame-layer must have inner_border element!");
    readBoundingBox(json.asString(), itsInnerBorder);

    json = JsonTools::remove(jdim, "outer_border");
    if (!json.isNull())
    {
      itsOuterBorder = FrameDimension();
      readBoundingBox(json.asString(), *itsOuterBorder);
    }

    auto jscale = JsonTools::remove(theJson, "scale");

    if (!jscale.isNull())
    {
      itsScale = FrameScale();
      itsScale->ticPosition = "outside";
      JsonTools::remove_string(itsScale->ticPosition, jscale, "tic_position");
      itsScale->labelPosition = "outside";
      JsonTools::remove_double(itsScale->labelStep, jscale, "label_step");
      JsonTools::remove_string(itsScale->labelPosition, jscale, "label_position");

      if (jscale.isMember("small_tic_step"))
      {
        itsScale->smallTic = TicInfo();
        JsonTools::remove_double(itsScale->smallTic->step, jscale, "small_tic_step");
        JsonTools::remove_int(itsScale->smallTic->length, jscale, "small_tic_length");
      }
      if (jscale.isMember("intermediate_tic_step"))
      {
        itsScale->intermediateTic = TicInfo();
        JsonTools::remove_double(itsScale->intermediateTic->step, jscale, "intermediate_tic_step");
        JsonTools::remove_int(itsScale->intermediateTic->length, jscale, "intermediate_tic_length");
      }
      if (jscale.isMember("long_tic_step"))
      {
        itsScale->longTic = TicInfo();
        JsonTools::remove_double(itsScale->longTic->step, jscale, "long_tic_step");
        JsonTools::remove_int(itsScale->longTic->length, jscale, "long_tic_length");
      }
    }
    itsGeom = createFrameGeometry(itsInnerBorder, itsOuterBorder);
    Fmi::SpatialReference epsg4326("EPSG:4326");
    itsGeom->assignSpatialReference(epsg4326.get());
    itsGeom->transformTo(projection.getCRS());
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

void FrameLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    const auto box = projection.getBox();
    const auto& crs = projection.getCRS();

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    if (!theState.inDefs())
    {
      group_cdt["start"] = "<g";
      group_cdt["end"] = "</g>";
      // Add attributes to the group
      theState.addAttributes(theGlobals, group_cdt, attributes);
    }

    // Store the path with unique ID
    //    std::string iri = Fmi::to_string(937);
    std::string iri = qid;

    CTPP::CDT frame_cdt(CTPP::CDT::HASH_VAL);
    frame_cdt["iri"] = iri;
    frame_cdt["type"] = Geometry::name(*itsGeom, theState.getType());

    frame_cdt["layertype"] = "frame";
    frame_cdt["data"] = Geometry::toString(*itsGeom, theState.getType(), box, crs, itsPrecision);
    //    theState.addPresentationAttributes(frame_cdt, css, attributes);
    theGlobals["paths"][iri] = frame_cdt;

    // add pattern on geometry
    std::string pattern_iri = (itsPattern ? *itsPattern : "");
    if (!pattern_iri.empty())
    {
      if (theState.addId(pattern_iri))
        theGlobals["includes"][pattern_iri] = theState.getPattern(pattern_iri);
    }

    if (!theState.inDefs())
    {
      // Add the SVG use element
      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";
      theState.addAttributes(theGlobals, tag_cdt, attributes);
      tag_cdt["attributes"]["xlink:href"] = "#" + iri;
      group_cdt["tags"].PushBack(tag_cdt);
    }
    theLayersCdt.PushBack(group_cdt);
    addScale(theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void FrameLayer::addScaleNumber(CTPP::CDT& theLayersCdt, double x, double y, const std::string& num)
{
  // add coordinate number
  CTPP::CDT textCdt(CTPP::CDT::HASH_VAL);
  textCdt["start"] = "<text";
  textCdt["end"] = "</text>";
  textCdt["cdata"] = Fmi::safexmlescape(num);
  textCdt["attributes"]["x"] = Fmi::to_string(lround(x));
  textCdt["attributes"]["y"] = Fmi::to_string(lround(y));
  textCdt["attributes"]["font-family"] = itsScaleAttributes.value("font-family");
  textCdt["attributes"]["font-size"] = itsScaleAttributes.value("font-size");
  textCdt["attributes"]["font-style"] = itsScaleAttributes.value("font-style");
  textCdt["attributes"]["font-weight"] = itsScaleAttributes.value("font-weight");
  textCdt["attributes"]["text-anchor"] = itsScaleAttributes.value("text-anchor");
  theLayersCdt.PushBack(textCdt);
}

void FrameLayer::addTic(CTPP::CDT& theLayersCdt, double x1, double y1, double x2, double y2)
{
  CTPP::CDT lineCdt(CTPP::CDT::HASH_VAL);
  lineCdt["start"] = "<line";
  lineCdt["end"] = "</line>";
  lineCdt["attributes"]["x1"] = Fmi::to_string(x1);
  lineCdt["attributes"]["y1"] = Fmi::to_string(y1);
  lineCdt["attributes"]["x2"] = Fmi::to_string(x2);
  lineCdt["attributes"]["y2"] = Fmi::to_string(y2);
  lineCdt["attributes"]["stroke"] = itsScaleAttributes.value("stroke");
  lineCdt["attributes"]["stroke-width"] = itsScaleAttributes.value("stroke-width");

  theLayersCdt.PushBack(lineCdt);
}

void FrameLayer::addLatitudeTics(CTPP::CDT& theLayersCdt,
                                 LonLatToXYTransformation& transformation,
                                 const std::map<double, TicInfo>& latitudeTics,
                                 const text_style_t& labelStyle,
                                 bool ticIsOutside,
                                 bool labelIsOutside)
{
  for (const auto& tic : latitudeTics)
  {
    // left hand side
    double lonStartLeft = itsInnerBorder.leftLongitude;
    double latStart = tic.first;
    if (!transformation.transform(lonStartLeft, latStart))
      continue;

    double lonEnd = lonStartLeft + (ticIsOutside ? -tic.second.length : tic.second.length);
    addTic(theLayersCdt, lonStartLeft, latStart, lonEnd, latStart);

    // right hand side
    double lonStartRight = itsInnerBorder.rightLongitude;
    latStart = tic.first;
    transformation.transform(lonStartRight, latStart);
    lonEnd = lonStartRight + (ticIsOutside ? tic.second.length : -tic.second.length);
    addTic(theLayersCdt, lonStartRight, latStart, lonEnd, latStart);

    // add labels
    if (at_label(tic.first, itsScale->labelStep))
    {
      std::string labelText = (Fmi::to_string(lround(tic.first)) + "°");
      text_dimension_t labelDimension = getTextDimension(labelText, labelStyle);

      latitude_label_offsets(labelIsOutside,
                             ticIsOutside,
                             labelDimension.width,
                             tic.second.length,
                             lonStartLeft,
                             lonStartRight);
      latStart += (labelDimension.height / 2.0);
      addScaleNumber(theLayersCdt, lonStartLeft, latStart, labelText);
      addScaleNumber(theLayersCdt, lonStartRight, latStart, labelText);
    }
  }
}

void FrameLayer::addLongitudeTics(CTPP::CDT& theLayersCdt,
                                  LonLatToXYTransformation& transformation,
                                  const std::map<double, TicInfo>& longitudeTics,
                                  const text_style_t& labelStyle,
                                  bool ticIsOutside,
                                  bool labelIsOutside)
{
  for (const auto& tic : longitudeTics)
  {
    // top
    double lonStart = tic.first;
    double latStartTop = itsInnerBorder.topLatitude;  // latMax;
    if (!transformation.transform(lonStart, latStartTop))
      continue;

    double latEnd = latStartTop + (ticIsOutside ? -tic.second.length : tic.second.length);
    addTic(theLayersCdt, lonStart, latStartTop, lonStart, latEnd);

    // bottom
    lonStart = tic.first;
    double latStartBottom = itsInnerBorder.bottomLatitude;  // latMin;

    if (!transformation.transform(lonStart, latStartBottom))
      continue;

    latEnd = latStartBottom + (ticIsOutside ? tic.second.length : -tic.second.length);
    addTic(theLayersCdt, lonStart, latStartBottom, lonStart, latEnd);

    // add labels
    if (at_label(tic.first, itsScale->labelStep))
    {
      std::string labelText = (Fmi::to_string(lround(tic.first)));
      text_dimension_t labelDimension = getTextDimension(labelText, labelStyle);
      labelText += "°";

      longitude_label_offsets(labelIsOutside,
                              ticIsOutside,
                              labelDimension.height,
                              tic.second.length,
                              latStartTop,
                              latStartBottom);
      lonStart -= (labelDimension.width / 2.0);
      addScaleNumber(theLayersCdt, lonStart, latStartTop, labelText);
      addScaleNumber(theLayersCdt, lonStart, latStartBottom, labelText);
    }
  }
}

void FrameLayer::addScale(CTPP::CDT& theLayersCdt, const State& /* theState */)
{
  if (!itsScale)
    return;
  auto transformation = LonLatToXYTransformation(projection);

  int lonMax = ceil(itsInnerBorder.rightLongitude);
  int latMax = ceil(itsInnerBorder.topLatitude);
  int lonMin = floor(itsInnerBorder.leftLongitude);
  int latMin = floor(itsInnerBorder.bottomLatitude);

  text_style_t labelStyle;
  labelStyle.fontfamily = itsScaleAttributes.value("font-family");
  labelStyle.fontsize = itsScaleAttributes.value("font-size");
  labelStyle.fontstyle = itsScaleAttributes.value("font-style");
  labelStyle.fontweight = itsScaleAttributes.value("font-weight");
  labelStyle.textanchor = itsScaleAttributes.value("text-anchor");

  const auto latitudeTics = collect_tics(
      *itsScale, latMin, latMax, itsInnerBorder.bottomLatitude, itsInnerBorder.topLatitude);

  bool ticIsOutside = (itsScale->ticPosition == "outside");
  bool labelIsOutside = (*itsScale->labelPosition == "outside");

  addLatitudeTics(
      theLayersCdt, transformation, latitudeTics, labelStyle, ticIsOutside, labelIsOutside);

  const auto longitudeTics = collect_tics(
      *itsScale, lonMin, lonMax, itsInnerBorder.leftLongitude, itsInnerBorder.rightLongitude);

  addLongitudeTics(
      theLayersCdt, transformation, longitudeTics, labelStyle, ticIsOutside, labelIsOutside);
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo
 */
// ----------------------------------------------------------------------

void FrameLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
  // no info available
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t FrameDimension::hash_value() const
{
  std::size_t hash = 0;

  Fmi::hash_combine(hash, Fmi::hash_value(bottomLatitude));
  Fmi::hash_combine(hash, Fmi::hash_value(leftLongitude));
  Fmi::hash_combine(hash, Fmi::hash_value(topLatitude));
  Fmi::hash_combine(hash, Fmi::hash_value(rightLongitude));

  return hash;
}

std::size_t TicInfo::hash_value() const
{
  std::size_t hash = 0;

  Fmi::hash_combine(hash, Fmi::hash_value(step));
  Fmi::hash_combine(hash, Fmi::hash_value(length));

  return hash;
}

std::size_t FrameScale::hash_value() const
{
  std::size_t hash = 0;

  Fmi::hash_combine(hash, dimension.hash_value());
  if (smallTic)
    Fmi::hash_combine(hash, smallTic->hash_value());
  if (intermediateTic)
    Fmi::hash_combine(hash, intermediateTic->hash_value());
  if (longTic)
    Fmi::hash_combine(hash, longTic->hash_value());
  Fmi::hash_combine(hash, Fmi::hash_value(ticPosition));
  Fmi::hash_combine(hash, Fmi::hash_value(labelStep));
  Fmi::hash_combine(hash, Fmi::hash_value(labelPosition));

  return hash;
}

std::size_t FrameLayer::hash_value(const State& theState) const
{
  try
  {
    std::size_t hash = 0;
    Fmi::hash_combine(hash, Layer::hash_value(theState));
    Fmi::hash_combine(hash, Fmi::hash_value(itsPrecision));
    Fmi::hash_combine(hash, itsInnerBorder.hash_value());
    if (itsOuterBorder)
      Fmi::hash_combine(hash, itsOuterBorder->hash_value());
    if (itsScale)
      Fmi::hash_combine(hash, itsScale->hash_value());
    Fmi::hash_combine(hash, Dali::hash_value(itsScaleAttributes, theState));

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
