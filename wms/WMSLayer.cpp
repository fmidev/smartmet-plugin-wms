#include "WMS.h"
#include "WMSLayer.h"
#include "WMSException.h"

#include <engines/gis/Engine.h>
#include <spine/Exception.h>

#include <macgyver/TimeParser.h>
#include <macgyver/StringConversion.h>

#include <boost/foreach.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>

#include <fmt/format.h>

#include <gdal/ogr_spatialref.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
// Remember to run updateLayerMetaData before using the layer!
WMSLayer::WMSLayer()
{
  geographicBoundingBox.xMin = 0.0;
  geographicBoundingBox.xMax = 0.0;
  geographicBoundingBox.yMin = 0.0;
  geographicBoundingBox.yMax = 0.0;
}
std::string WMSLayer::getName() const
{
  return name;
}
std::string WMSLayer::getCustomer() const
{
  return customer;
}
bool WMSLayer::isValidCRS(const std::string& theCRS) const
{
  try
  {
    if (crs.find(theCRS) != crs.end())
      return true;

    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSLayer::isValidStyle(const std::string& theStyle) const
{
  try
  {
    if (styles.find(theStyle) != styles.end())
      return true;

    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSLayer::isValidTime(const boost::posix_time::ptime& theTime) const
{
  try
  {
    if (timeDimension)
      return timeDimension->isValidTime(theTime);
    else
      return true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSLayer::currentValue() const
{
  try
  {
    // if time dimension is not present, time-option is irrelevant
    return (timeDimension ? timeDimension->currentValue() : true);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

boost::posix_time::ptime WMSLayer::mostCurrentTime() const
{
  try
  {
    return (timeDimension ? timeDimension->mostCurrentTime() : boost::posix_time::not_a_date_time);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSLayer::info() const
{
  try
  {
    std::stringstream ss;

    ss << *this;

    return ss.str();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::ostream& operator<<(std::ostream& ost, const WMSLayer& layer)
{
  try
  {
    ost << "********** "
        << "name: " << layer.name << " **********" << std::endl
        << "title: " << layer.title << std::endl
        << "abstract: " << layer.abstract << std::endl
        << "queryable: " << (layer.queryable ? "true" : "false") << std::endl
        << "keywords: " << boost::algorithm::join(layer.keywords, " ") << std::endl
        << "geographicBoundingBox: " << std::endl
        << " westBoundLongitude=" << layer.geographicBoundingBox.xMin << " "
        << "southBoundLatitude=" << layer.geographicBoundingBox.yMin << " "
        << "eastBoundLongitude=" << layer.geographicBoundingBox.xMax << " "
        << "northBoundLatitude=" << layer.geographicBoundingBox.yMax << std::endl
        << "crs: " << boost::algorithm::join(layer.crs, " ") << std::endl
        << "styles: " << boost::algorithm::join(layer.styles, " ") << std::endl
        << "customer: " << layer.customer << std::endl;

    if (layer.timeDimension)
    {
      ost << "timeDimension: " << layer.timeDimension;
    }
    else
    {
      ost << "timeDimension: Empty";
    }

    ost << "*******************************" << std::endl;

    return ost;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSLayer::generateGetCapabilities(const Engine::Gis::Engine& gisengine)
{
  try
  {
    std::string ss = std::string(" <Layer queryable=") +
                     enclose_with_quotes(queryable ? "1" : "0") + " opaque=\"1\">" + "\n" +
                     "   <Name>" + name + "</Name>" + "\n" + "   <Title>" + title + "</Title>" +
                     "\n" + "   <Abstract>" + abstract + "</Abstract>" + "\n";

    std::vector<std::string> supportedCRS;

    double lonMin = geographicBoundingBox.xMin;
    double lonMax = geographicBoundingBox.xMax;
    double latMin = geographicBoundingBox.yMin;
    double latMax = geographicBoundingBox.yMax;

    // EX_GeographicBoundingBox
    std::string bbox_ss =
        std::string("   <EX_GeographicBoundingBox>") + "\n" + "\t<westBoundLongitude>" +
        Fmi::to_string(lonMin) + "</westBoundLongitude>" + "\n" + "\t<eastBoundLongitude>" +
        Fmi::to_string(lonMax) + "</eastBoundLongitude>" + "\n" + "\t<southBoundLatitude>" +
        Fmi::to_string(latMin) + "</southBoundLatitude>" + "\n" + "\t<northBoundLatitude>" +
        Fmi::to_string(latMax) + "</northBoundLatitude>" + "\n" + "   </EX_GeographicBoundingBox>" +
        "\n";

    bbox_ss += "   <BoundingBox CRS=" + enclose_with_quotes("EPSG:4326");
    bbox_ss += " minx=\"" + Fmi::to_string(latMin) + "\"";
    bbox_ss += " miny=\"" + Fmi::to_string(lonMin) + "\"";
    bbox_ss += " maxx=\"" + Fmi::to_string(latMax) + "\"";
    bbox_ss += " maxy=\"" + Fmi::to_string(lonMax) + "\"";
    bbox_ss += " />\n";

    // crs bounding boxes
    // convert lon/lat-coordinated to crs coordinates
    for (std::set<std::string>::const_iterator iter = crs.begin(); iter != crs.end(); iter++)
    {
      std::string crs(*iter);

      if (crs == "EPSG:4326")
      {
        supportedCRS.push_back(crs);
        continue;
      }

      OGRSpatialReference oSourceSRS, oTargetSRS;

      double x1(lonMin), y1(latMin), x2(lonMax), y2(latMax);

      oSourceSRS.importFromEPSGA(4326);

      int targetEPSGNumber = stoi(iter->substr(5));
      OGRErr err = oTargetSRS.importFromEPSGA(targetEPSGNumber);

      if (err != OGRERR_NONE)
        throw Spine::Exception(BCP, "Unknown CRS: '" + crs + "'");

      boost::shared_ptr<OGRCoordinateTransformation> poCT(
          OGRCreateCoordinateTransformation(&oSourceSRS, &oTargetSRS));

      if (poCT == NULL)
        throw Spine::Exception(BCP, "OGRCreateCoordinateTransformation function call failed");

      // Intersect with target EPSG bounding box

      Engine::Gis::BBox epsg_box = gisengine.getBBox(targetEPSGNumber);
      x1 = std::max(epsg_box.west, x1);
      x2 = std::min(epsg_box.east, x2);
      y1 = std::max(epsg_box.south, y1);
      y2 = std::min(epsg_box.north, y2);

      if (x1 >= x2 || y1 >= y2)
      {
        // Non-overlapping bbox
        continue;
      }

      bool ok = (poCT->Transform(1, &x1, &y1) && poCT->Transform(1, &x2, &y2));
      if (!ok)
      {
        // Corner transformations failed
        continue;
      }

      supportedCRS.push_back(crs);

      bbox_ss += "   <BoundingBox CRS=" + enclose_with_quotes(crs);

      // check if coordinate ordering must be changed
      if (oTargetSRS.EPSGTreatsAsLatLong())
      {
        bbox_ss += " minx=\"" + fmt::sprintf("%f", y1) + "\"";
        bbox_ss += " miny=\"" + fmt::sprintf("%f", x1) + "\"";
        bbox_ss += " maxx=\"" + fmt::sprintf("%f", y2) + "\"";
        bbox_ss += " maxy=\"" + fmt::sprintf("%f", x2) + "\"";
      }
      else
      {
        bbox_ss += " minx=\"" + fmt::sprintf("%f", x1) + "\"";
        bbox_ss += " miny=\"" + fmt::sprintf("%f", y1) + "\"";
        bbox_ss += " maxx=\"" + fmt::sprintf("%f", x2) + "\"";
        bbox_ss += " maxy=\"" + fmt::sprintf("%f", y2) + "\"";
      }

      bbox_ss += " />\n";
    }

    // first insert list of supported CRS
    BOOST_FOREACH (const std::string& crs, supportedCRS)
      ss += "   <CRS>" + crs + "</CRS>\n";

    ss += bbox_ss;

    if (timeDimension)
    {
      // then bounding boxes of these CRS
      ss += "   <Dimension name=\"time\" units=" + enclose_with_quotes("ISO8601") +
            "  multipleValues=" + enclose_with_quotes("0") + "  nearestValue=" +
            enclose_with_quotes("0") + "  current=" +
            enclose_with_quotes(timeDimension->currentValue() ? "1" : "0") + ">" +
            timeDimension->getCapabilities() + "</Dimension>" + "\n";
    }

    ss += " </Layer>\n";

    return ss;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
