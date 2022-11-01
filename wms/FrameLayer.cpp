#include "FrameLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Layer.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include "TextUtility.h"
#include <ctpp2/CDT.hpp>
#include <gis/OGR.h>
#include <macgyver/Exception.h>
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

OGRGeometryPtr createFrameGeometry(const FrameDimension& inner,
                                   boost::optional<FrameDimension> outer)
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
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------
void FrameLayer::init(const Json::Value& theJson,
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
    Json::Value nulljson;

    auto jsonScaleAttributes = theJson.get("scale_attributes", nulljson);
    if (!jsonScaleAttributes.isNull())
      itsScaleAttributes.init(jsonScaleAttributes, theConfig);

    auto jsonDimension = theJson.get("dimension", nulljson);
    if (jsonDimension.isNull())
      throw Fmi::Exception(BCP, "Frame-layer must have dimension element!");

    auto jsonPrecision = theJson.get("precision", nulljson);
    if (!jsonPrecision.isNull())
      itsPrecision = jsonPrecision.asDouble();

    auto json = jsonDimension.get("inner_border", nulljson);
    if (json.isNull())
      throw Fmi::Exception(BCP, "Frame-layer must have inner_border element!");
    std::string innerBorder = json.asString();
    readBoundingBox(innerBorder, itsInnerBorder);

    json = theJson.get("pattern", nulljson);
    if (!json.isNull())
      itsPattern = json.asString();

    json = jsonDimension.get("outer_border", nulljson);
    if (!json.isNull())
    {
      std::string outerBorder = json.asString();
      itsOuterBorder = FrameDimension();
      readBoundingBox(outerBorder, *itsOuterBorder);
    }

    json = jsonDimension.get("outer_border", nulljson);

    auto jsonScale = theJson.get("scale", nulljson);
    if (!json.isNull())
    {
      itsScale = FrameScale();
      itsScale->ticPosition = "outside";
      json = jsonScale.get("tic_position", nulljson);
      if (!json.isNull())
        itsScale->ticPosition = json.asString();

      json = jsonScale.get("small_tic_step", nulljson);
      if (!json.isNull())
      {
        itsScale->smallTic = TicInfo();
        itsScale->smallTic->step = json.asDouble();
        json = jsonScale.get("small_tic_length", nulljson);
        if (!json.isNull())
          itsScale->smallTic->length = json.asInt();
      }
      json = jsonScale.get("intermediate_tic_step", nulljson);
      if (!json.isNull())
      {
        itsScale->intermediateTic = TicInfo();
        itsScale->intermediateTic->step = json.asDouble();
        json = jsonScale.get("intermediate_tic_length", nulljson);
        if (!json.isNull())
          itsScale->intermediateTic->length = json.asInt();
      }
      json = jsonScale.get("long_tic_step", nulljson);
      if (!json.isNull())
      {
        itsScale->longTic = TicInfo();
        itsScale->longTic->step = json.asDouble();
        json = jsonScale.get("long_tic_length", nulljson);
        if (!json.isNull())
          itsScale->longTic->length = json.asInt();
      }
      json = jsonScale.get("label_step", nulljson);
      if (!json.isNull())
        itsScale->labelStep = json.asDouble();
      json = jsonScale.get("label_position", nulljson);
      if (!json.isNull())
        itsScale->labelPosition = json.asString();
      else
        itsScale->labelPosition = "outside";
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
  textCdt["cdata"] = num;
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

  std::map<double, TicInfo> latitudeTics;
  // first add long tics
  if (itsScale->longTic)
  {
    double lat = latMin + itsScale->longTic->step;
    while (lat < latMax)
    {
      if (lat > itsInnerBorder.bottomLatitude && lat < itsInnerBorder.topLatitude)
        latitudeTics.insert(std::make_pair(lat, *(itsScale->longTic)));
      lat += itsScale->longTic->step;
    }
  }

  if (itsScale->intermediateTic)
  {
    double lat = latMin + itsScale->intermediateTic->step;
    while (lat < latMax)
    {
      if (itsScale->longTic && fmod(lat, itsScale->longTic->step) > 0.01 &&
          lat > itsInnerBorder.bottomLatitude && lat < itsInnerBorder.topLatitude)
        latitudeTics.insert(std::make_pair(lat, *(itsScale->intermediateTic)));
      lat += itsScale->intermediateTic->step;
    }
  }

  if (itsScale->smallTic)
  {
    // add small tic, if long or intermediate tic does not exist at the same latitude
    double lat = latMin + itsScale->smallTic->step;
    while (lat < latMax)
    {
      if (itsScale->longTic && fmod(lat, itsScale->longTic->step) > 0.01 &&
          itsScale->intermediateTic && fmod(lat, itsScale->intermediateTic->step) > 0.01 &&
          lat > itsInnerBorder.bottomLatitude && lat < itsInnerBorder.topLatitude)
        latitudeTics.insert(std::make_pair(lat, *(itsScale->smallTic)));
      lat += itsScale->smallTic->step;
    }
  }

  bool ticIsOutside = (itsScale->ticPosition == "outside");
  bool labelIsOutside = (*itsScale->labelPosition == "outside");

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
    if (fmod(tic.first, *(itsScale->labelStep)) < 0.01)
    {
      std::string labelText = (Fmi::to_string(lround(tic.first)) + "°");
      text_dimension_t labelDimension = getTextDimension(labelText, labelStyle);

      if (labelIsOutside)
      {
        if (ticIsOutside)  // label out, tic out
        {
          lonStartLeft -= (labelDimension.width + (tic.second.length * 1.3));
          lonStartRight += (tic.second.length * 1.3);
        }
        else  // label out, tic in
        {
          lonStartLeft -= (labelDimension.width + (tic.second.length * 0.3));
          lonStartRight += (labelDimension.width * +0.3);
        }
      }
      else
      {
        if (ticIsOutside)  // label in, tic out
        {
          lonStartLeft += (labelDimension.width * 0.3);
          lonStartRight -= (labelDimension.width * 1.3);
        }
        else  // label in, tic in
        {
          lonStartLeft += (tic.second.length * 1.3);
          lonStartRight -= (labelDimension.width + (tic.second.length * 1.3));
        }
      }
      latStart += (labelDimension.height / 2.0);
      addScaleNumber(theLayersCdt, lonStartLeft, latStart, labelText);
      addScaleNumber(theLayersCdt, lonStartRight, latStart, labelText);
    }
  }

  std::map<double, TicInfo> longitudeTics;
  // first add long tics
  if (itsScale->longTic)
  {
    double lon = lonMin + itsScale->longTic->step;
    while (lon < lonMax)
    {
      if (lon > itsInnerBorder.leftLongitude && lon < itsInnerBorder.rightLongitude)
        longitudeTics.insert(std::make_pair(lon, *(itsScale->longTic)));
      lon += itsScale->longTic->step;
    }
  }

  if (itsScale->intermediateTic)
  {
    // add intermediate tic, if long tic does not exist at the same longitude
    double lon = lonMin + itsScale->intermediateTic->step;
    while (lon < lonMax)
    {
      if (itsScale->longTic && fmod(lon, itsScale->longTic->step) > 0.01 &&
          lon > itsInnerBorder.leftLongitude && lon < itsInnerBorder.rightLongitude)
        longitudeTics.insert(std::make_pair(lon, *(itsScale->intermediateTic)));
      lon += itsScale->intermediateTic->step;
    }
  }

  if (itsScale->smallTic)
  {
    // add small tic, if long or intermediate tic does not exist at the same longitude
    double lon = lonMin + itsScale->smallTic->step;
    while (lon < lonMax)
    {
      if (itsScale->longTic && fmod(lon, itsScale->longTic->step) > 0.01 &&
          itsScale->intermediateTic && fmod(lon, itsScale->intermediateTic->step) > 0.01 &&
          lon > itsInnerBorder.leftLongitude && lon < itsInnerBorder.rightLongitude)
        longitudeTics.insert(std::make_pair(lon, *(itsScale->smallTic)));
      lon += itsScale->smallTic->step;
    }
  }

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
    if (fmod(tic.first, *(itsScale->labelStep)) < 0.01)
    {
      std::string labelText = (Fmi::to_string(lround(tic.first)));
      text_dimension_t labelDimension = getTextDimension(labelText, labelStyle);
      labelText += "°";

      if (labelIsOutside)
      {
        if (ticIsOutside)  // label out, tic out
        {
          latStartTop -= (tic.second.length * 1.3);
          latStartBottom += (labelDimension.height + (tic.second.length * 1.3));
        }
        else  // label out, tic in
        {
          latStartTop -= (labelDimension.height * 0.3);
          latStartBottom += (labelDimension.height * 1.3);
        }
      }
      else
      {
        if (ticIsOutside)  // label in tic out
        {
          latStartTop += (labelDimension.height * 1.3);
          latStartBottom -= (labelDimension.height * 0.3);
        }
        else  // label in tic in
        {
          latStartTop += (labelDimension.height + (tic.second.length * 1.3));
          latStartBottom -= (tic.second.length * 1.3);
        }
      }
      lonStart -= (labelDimension.width / 2.0);
      addScaleNumber(theLayersCdt, lonStart, latStartTop, labelText);
      addScaleNumber(theLayersCdt, lonStart, latStartBottom, labelText);
    }
  }
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
