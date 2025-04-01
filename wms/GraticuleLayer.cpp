#include "GraticuleLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <macgyver/StringConversion.h>
#include <spine/Json.h>
#include <cmath>
#include <ogr_geometry.h>
#include <set>
#include <vector>

#include <fmt/printf.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{

// SVG/XML does not support HTML entities by default, it is easier to use numeric values
const std::string minus_char = "&#x2212;";  // same as &minus; in HTML
const std::string degree_char = "&#176;";   // same as &deg; in HTML

// Valid layouts for labels and lines

const std::set<std::string> valid_label_layouts = {
    "none", "center", "cross", "edge_center", "ticks"};

const std::set<std::string> valid_line_layouts = {"grid", "ticks"};

using Graticule = GraticuleLayer::Graticule;
using GraticuleLabels = GraticuleLayer::GraticuleLabels;

struct GraticuleLines
{
  std::unique_ptr<OGRGeometry> labeled;  // lines which will have labels due to labels.step

  std::unique_ptr<OGRGeometry> unlabeled;  // lines which will not have labels since layout=none or
                                           // graticule.step and labels.step do not match
};

// Helper structure for a single graticule label
struct Label
{
  Label(std::string l_, double x_, double y_, double a_)
      : text(std::move(l_)), x(x_), y(y_), angle(a_)
  {
  }

  std::string text;
  double x = 0;
  double y = 0;
  double angle = 0;
};

using Labels = std::vector<Label>;

// ----------------------------------------------------------------------
/*!
 * \brief If there is an intersection produce a tick
 */
// ----------------------------------------------------------------------

OGRLineString* tick(double x1,
                    double x2,
                    double y1,
                    double y2,
                    double x,
                    const Fmi::Box& box,
                    bool xy,
                    unsigned int length)
{
  if (x1 == x2)  // safety check
    return nullptr;

  // Intersection coordinate

  auto y = y1 + (x - x1) / (x2 - x1) * (y2 - y1);

  // Verify the intersection is within the bounding box
  if (xy)
  {
    if (y < box.ymin() || y > box.ymax())
      return nullptr;
  }
  else
  {
    if (y < box.xmin() || y > box.xmax())
      return nullptr;
  }

  // Now produce a tick of desired length in the direction of the x1,y1 - x2,y2 line starting at
  // x,y. First we need to project x,y - x2,y2 to pixel coordinates and scale x2,y2 to be at the
  // proper distance, and then we need to project the end point back to metric coordinates

  double x_start = x;
  double y_start = y;
  double x_end = x2;
  double y_end = y2;
  box.transform(x_start, y_start);
  box.transform(x_end, y_end);

  auto dx = x_end - x_start;
  auto dy = y_end - y_start;
  auto len = std::hypot(dx, dy);

  if (len == 0)
    return nullptr;  // safety check against very small rounding errors

  x_end = x_start + dx / len * length;
  y_end = y_start + dy / len * length;

  box.itransform(x_end, y_end);

  // Swap x/y coordinates if necessary

  if (!xy)
  {
    std::swap(x, y);
    std::swap(x_end, y_end);
  }

  auto xx1 = x;
  auto yy1 = y;
  auto xx2 = x_end;
  auto yy2 = y_end;
  box.transform(xx1, yy1);
  box.transform(xx2, yy2);

  // Return the tick
  auto* line = new OGRLineString;
  line->addPoint(x, y);
  line->addPoint(x_end, y_end);
  return line;
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the value should be skipped for being in the "except" list
 */
// ----------------------------------------------------------------------

bool skip_value(int value, const std::vector<int>& except_vector)
{
  if (except_vector.empty())
    return false;

  for (const auto& except : except_vector)
  {
    if (except != 0 && std::abs(value) % except == 0)
      return true;
  }
  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given graticule latitude or longitude will be labeled
 */
// ----------------------------------------------------------------------

bool is_labeled(int value, const Graticule& graticule)
{
  if (graticule.labels.layout == "none")
    return false;
  return (std::abs(value) % graticule.labels.step == 0);
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate longitude label
 */
// ----------------------------------------------------------------------

std::string get_lon_label(const GraticuleLabels& label, int lon)
{
  try
  {
    std::string txt;
    if (label.minus_sign && lon < 0)
      txt += minus_char;
    txt += Fmi::to_string(std::abs(lon));
    if (label.degree_sign)
      txt += degree_char;
    if (!label.minus_sign)
    {
      if (lon < 0)
        txt += "W";
      else if (lon > 0)
        txt += "E";
    }
    return txt;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate latitude label
 */
// ----------------------------------------------------------------------

std::string get_lat_label(const GraticuleLabels& label, int lat)
{
  try
  {
    std::string txt;
    if (label.minus_sign && lat < 0)
      txt += minus_char;
    txt += Fmi::to_string(std::abs(lat));
    if (label.degree_sign)
      txt += degree_char;
    if (!label.minus_sign)
    {
      if (lat < 0)
        txt += "S";
      else if (lat > 0)
        txt += "N";
    }
    return txt;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule labels at the centers of graticule lines
 */
// ----------------------------------------------------------------------

Labels generate_labels_center(const Graticule& g,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box)
{
  try
  {
    Labels labels;
    const auto& l = g.labels;

    // Generate the latlon coordinates first

    std::vector<double> x;
    std::vector<double> y;
    std::vector<std::string> txt;

    // Centers of vertical lines
    for (int lon = -180; lon <= 180; lon += l.step)
      for (int lat = -90; lat < 90; lat += l.step)
      {
        x.push_back(lon);
        y.push_back(lat + l.step / 2.0);
        txt.push_back(get_lon_label(l, lon));
      }

    // Centers of horizontal lines
    for (int lat = -90 + l.step; lat < 90; lat += l.step)
      for (int lon = -180; lon <= 180; lon += l.step)
      {
        x.push_back(lon + l.step / 2.0);
        y.push_back(lat);
        txt.emplace_back(get_lat_label(l, lat));
      }

    // Convert to world coordinates
    Fmi::CoordinateTransformation transformation("WGS84", crs);
    transformation.transform(x, y);

    // Convert to pixel coordinates
    for (auto i = 0UL; i < x.size(); i++)
      box.transform(x[i], y[i]);

    // Generate the labels

    for (auto i = 0UL; i < x.size(); i++)
    {
      if (x[i] > 0 && x[i] < box.width() && y[i] > 0 && y[i] < box.height())
        labels.emplace_back(txt[i], x[i], y[i], 0);
    }

    return labels;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule labels at the outermost centers of graticule lines
 *
 * For each horizontal and vertical line we find the outermost coordinates (if any)
 * which are inside the image, and keep only those two.
 */
// ----------------------------------------------------------------------

Labels generate_labels_edge_center(const Graticule& g,
                                   const Fmi::SpatialReference& crs,
                                   const Fmi::Box& box)
{
  try
  {
    Labels labels;
    const auto& l = g.labels;

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    std::vector<double> x;
    std::vector<double> y;

    // Centers of vertical lines
    for (int lon = -180; lon <= 180; lon += l.step)
    {
      // latlon coordinates
      for (int lat = -90; lat < 90; lat += l.step)
      {
        x.push_back(lon);
        y.push_back(lat + l.step / 2.0);
      }

      // Convert to world coordinates
      transformation.transform(x, y);

      // Convert to pixel coordinates
      for (auto i = 0UL; i < x.size(); i++)
        box.transform(x[i], y[i]);

      auto minpos = std::string::npos;
      auto maxpos = std::string::npos;
      for (auto i = 1UL; i < y.size(); i++)
      {
        // discard points outside the box
        if (x[i] > 0 && x[i] < box.width() && y[i] > 0 && y[i] < box.height())
        {
          if (minpos == std::string::npos || y[i] < y[minpos])
            minpos = i;
          if (maxpos == std::string::npos || y[i] > y[maxpos])
            maxpos = i;
        }
      }

      if (minpos != std::string::npos)
      {
        auto txt = get_lon_label(l, lon);
        labels.push_back({txt, x[minpos], y[minpos], 0});

        if (minpos != maxpos)
          labels.emplace_back(txt, x[maxpos], y[maxpos], 0);
      }

      x.clear();
      y.clear();
    }

    // Centers of horizontal lines
    for (int lat = -90 + l.step; lat < 90; lat += l.step)
    {
      for (int lon = -180; lon < 180; lon += l.step)
      {
        x.push_back(lon + l.step / 2.0);
        y.push_back(lat);
      }

      // Convert to world coordinates
      transformation.transform(x, y);

      // Convert to pixel coordinates
      for (auto i = 0UL; i < x.size(); i++)
        box.transform(x[i], y[i]);

      auto minpos = std::string::npos;
      auto maxpos = std::string::npos;
      for (auto i = 1UL; i < x.size(); i++)
      {
        // discard points outside the box
        if (x[i] > 0 && x[i] < box.width() && y[i] > 0 && y[i] < box.height())
        {
          if (minpos == std::string::npos || x[i] < x[minpos])
            minpos = i;
          if (maxpos == std::string::npos || x[i] > x[maxpos])
            maxpos = i;
        }
      }

      if (minpos != std::string::npos)
      {
        auto txt = get_lat_label(l, lat);
        labels.push_back({txt, x[minpos], y[minpos], 0});

        if (minpos != maxpos)
          labels.emplace_back(txt, x[maxpos], y[maxpos], 0);
      }

      x.clear();
      y.clear();
    }

    return labels;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule labels at lines which would display most of them
 */
// ----------------------------------------------------------------------

Labels generate_labels_cross(const Graticule& g,
                             const Fmi::SpatialReference& crs,
                             const Fmi::Box& box)
{
  try
  {
    Labels labels;
    const auto& l = g.labels;

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    std::vector<double> x_best;
    std::vector<double> y_best;
    std::vector<std::string> txt_best;

    // Choose longitude containing most valid label positions at centers of lines

    for (int lon = -180; lon <= 180; lon += l.step)
    {
      std::vector<double> x;
      std::vector<double> y;
      std::vector<std::string> txt;

      // latlon coordinates
      for (int lat = -90; lat < 90; lat += l.step)
      {
        x.push_back(lon + l.step / 2.0);
        y.push_back(lat);
        txt.emplace_back(get_lat_label(l, lat));
      }

      // Convert to world coordinates
      transformation.transform(x, y);

      // Convert to pixel coordinates
      for (auto i = 0UL; i < x.size(); i++)
        box.transform(x[i], y[i]);

      // Discard points outside the box

      std::vector<double> valid_x;
      std::vector<double> valid_y;
      std::vector<std::string> valid_txt;

      for (auto i = 0UL; i < x.size(); i++)
      {
        if (x[i] >= 0 && x[i] < box.width() && y[i] >= 0 && y[i] < box.height())
        {
          valid_x.push_back(x[i]);
          valid_y.push_back(y[i]);
          valid_txt.emplace_back(txt[i]);
        }
      }

      // Keep the first candidate with the most positions
      if (valid_x.size() > x_best.size())
      {
        x_best = std::move(valid_x);
        y_best = std::move(valid_y);
        txt_best = std::move(valid_txt);
      }
    }

    // Keep the selected longitude
    for (auto i = 0UL; i < x_best.size(); i++)
      labels.emplace_back(txt_best[i], x_best[i], y_best[i], 0);

    x_best.clear();
    y_best.clear();
    txt_best.clear();

    // Choose latitude containing most valid label positions at centers of lines

    for (int lat = -90 + l.step; lat < 90; lat += l.step)
    {
      std::vector<double> x;
      std::vector<double> y;
      std::vector<std::string> txt;

      for (int lon = -180; lon < 180; lon += l.step)
      {
        x.push_back(lon);
        y.push_back(lat + l.step / 2.0);
        txt.emplace_back(get_lon_label(l, lon));
      }

      // Convert to world coordinates
      transformation.transform(x, y);

      // Convert to pixel coordinates
      for (auto i = 0UL; i < x.size(); i++)
        box.transform(x[i], y[i]);

      // Discard points outside the box

      std::vector<double> valid_x;
      std::vector<double> valid_y;
      std::vector<std::string> valid_txt;

      for (auto i = 0UL; i < x.size(); i++)
      {
        if (x[i] >= 0 && x[i] < box.width() && y[i] >= 0 && y[i] < box.height())
        {
          valid_x.push_back(x[i]);
          valid_y.push_back(y[i]);
          valid_txt.emplace_back(txt[i]);
        }
      }

      // Keep the first candidate with the most positions
      if (valid_x.size() > x_best.size())
      {
        x_best = std::move(valid_x);
        y_best = std::move(valid_y);
        txt_best = std::move(valid_txt);
      }
    }

    // Keep the selected longitude
    for (auto i = 0UL; i < x_best.size(); i++)
      labels.emplace_back(txt_best[i], x_best[i], y_best[i], 0);

    return labels;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule labels at tick end points
 */
// ----------------------------------------------------------------------

Labels generate_labels_ticks(const Graticule& g,
                             const Fmi::SpatialReference& crs,
                             const Fmi::Box& box)
{
  try
  {
    Labels labels;
    const auto& l = g.labels;

    // Similar code as in generate_ticks, we just shift the label based on dx,dy

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    // 0.5 degrees is usually good enough for meteorological data, 1.0 degrees may
    // produce ~1 km errors
    const double linestep = 1.0;

    std::vector<double> x;
    std::vector<double> y;

    // Add vertical ticks only at top and bottom edges
    for (int lon = -180; lon <= 180; lon += g.step)
    {
      if (skip_value(lon, g.except))
        continue;
      if (!is_labeled(lon, g))
        continue;

      // latlon coordinates
      for (int i = 0;; i++)
      {
        auto lat = -90.0 + i * linestep;
        if (lat > 90)
          break;
        x.push_back(lon);
        y.push_back(lat);
      }

      // CRS coordinates
      transformation.transform(x, y);

      // Find intersections with bbox

      OGRLineString* line = nullptr;
      for (auto i = 0UL; i < x.size() - 1; i++)
      {
        bool bottom = (y[i] <= box.ymin() && y[i + 1] >= box.ymin());

        if (bottom)
          line = tick(y[i], y[i + 1], x[i], x[i + 1], box.ymin(), box, false, g.length);
        else if (y[i] <= box.ymax() && y[i + 1] >= box.ymax())
          line = tick(y[i + 1], y[i], x[i + 1], x[i], box.ymax(), box, false, g.length);

        if (line)
        {
          auto txt = get_lon_label(l, lon);
          auto x1 = line->getX(0);
          auto y1 = line->getY(0);
          auto x2 = line->getX(1);
          auto y2 = line->getY(1);
          box.transform(x1, y1);
          box.transform(x2, y2);

          if (bottom)  // because baseline setting does not work properly at least in librsvg
            y2 -= l.dy;
          else
            y2 += l.dy;

          labels.emplace_back(txt, x2, y2, 0);
          OGRFree(line);
          line = nullptr;
        }
      }
      x.clear();
      y.clear();
    }

    // Add horizontal ticks only at left and right edges
    for (int lat = -90 + g.step; lat < 90; lat += g.step)
    {
      if (skip_value(lat, g.except))
        continue;
      if (!is_labeled(lat, g))
        continue;

      // latlon coordinates
      for (int i = 0;; i++)
      {
        auto lon = -180 + i * linestep;
        if (lon > 180)
          break;
        x.push_back(lon);
        y.push_back(lat);
      }

      // CRS coordinates
      transformation.transform(x, y);

      // Find intersections with bbox

      OGRLineString* line = nullptr;
      for (auto i = 0UL; i < x.size() - 1; i++)
      {
        bool left = (x[i] <= box.xmin() && x[i + 1] >= box.xmin());
        if (left)
          line = tick(x[i], x[i + 1], y[i], y[i + 1], box.xmin(), box, true, g.length);
        else if (x[i] <= box.xmax() && x[i + 1] >= box.xmax())
          line = tick(x[i + 1], x[i], y[i + 1], y[i], box.xmax(), box, true, g.length);

        if (line)
        {
          auto txt = get_lat_label(l, lat);
          auto x1 = line->getX(0);
          auto y1 = line->getY(0);
          auto x2 = line->getX(1);
          auto y2 = line->getY(1);
          box.transform(x1, y1);
          box.transform(x2, y2);

          y2 -= l.dy;  // because baseline setting does not work properly at least in librsvg

          labels.emplace_back(txt, x2, y2, 0);
          OGRFree(line);
          line = nullptr;
        }
      }
      x.clear();
      y.clear();
    }

    return labels;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule labels
 */
// ----------------------------------------------------------------------

Labels generate_labels(const Graticule& g, const Fmi::SpatialReference& crs, const Fmi::Box& box)
{
  try
  {
    const auto& name = g.labels.layout;
    if (name == "none")
      return {};
    if (name == "center")
      return generate_labels_center(g, crs, box);
    if (name == "cross")
      return generate_labels_cross(g, crs, box);
    if (name == "edge_center")
      return generate_labels_edge_center(g, crs, box);
    if (name == "ticks")
      return generate_labels_ticks(g, crs, box);

    throw Fmi::Exception(BCP, "Unknown graticule label layout").addParameter("Name", name);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate geographic graticule lines
 */
// ----------------------------------------------------------------------

GraticuleLines generate_geographic_lines(const Graticule& g)
{
  try
  {
    auto labeled = std::make_unique<OGRMultiLineString>();
    auto unlabeled = std::make_unique<OGRMultiLineString>();

    // 0.5 degrees is usually good enough for meteorological data, 1.0 degrees may
    // produce ~1 km errors
    const double linestep = 1.0;

    // Add vertical lines
    for (int lon = -180; lon <= 180; lon += g.step)
    {
      if (skip_value(lon, g.except))
        continue;

      auto* line = new OGRLineString();
      for (int i = 0;; i++)
      {
        auto lat = -90.0 + i * linestep;
        if (lat > 90)
          break;
        line->addPoint(lon, lat);
      }

      if (is_labeled(lon, g))
        labeled->addGeometryDirectly(line);
      else
        unlabeled->addGeometryDirectly(line);
    }

    // Add horizontal lines, poles are not included
    for (int lat = -90 + g.step; lat < 90; lat += g.step)
    {
      if (skip_value(lat, g.except))
        continue;
      auto* line = new OGRLineString();
      for (int i = 0;; i++)
      {
        auto lon = -180 + i * linestep;
        if (lon > 180)
          break;
        line->addPoint(lon, lat);
      }

      if (is_labeled(lat, g))
        labeled->addGeometryDirectly(line);
      else
        unlabeled->addGeometryDirectly(line);
    }

    labeled->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());
    unlabeled->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());

    GraticuleLines glines;
    glines.labeled = std::move(labeled);
    glines.unlabeled = std::move(unlabeled);

    return glines;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule lines in image coordinates
 */
// ----------------------------------------------------------------------

GraticuleLines generate_grid(const Graticule& g,
                             const Fmi::SpatialReference& crs,
                             const Fmi::Box& box)
{
  try
  {
    // Global geographic grid

    auto glines = generate_geographic_lines(g);

    // Projected grid with possible interrupt cuts done

    Fmi::CoordinateTransformation transformation("WGS84", crs);
    glines.labeled.reset(transformation.transformGeometry(*glines.labeled));
    glines.unlabeled.reset(transformation.transformGeometry(*glines.unlabeled));

    if (glines.labeled)
      glines.labeled.reset(Fmi::OGR::lineclip(*glines.labeled, box));
    if (glines.unlabeled)
      glines.unlabeled.reset(Fmi::OGR::lineclip(*glines.unlabeled, box));

    return glines;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule ticks in image coordinates
 *
 * We need to find out for the bounding box coordinates where they intersect
 * with the desired meridians etc, and then produce small ticks at those
 * coordinates.
 */
// ----------------------------------------------------------------------

GraticuleLines generate_ticks(const Graticule& g,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box)
{
  try
  {
    Fmi::CoordinateTransformation transformation("WGS84", crs);

    auto labeled = std::make_unique<OGRMultiLineString>();
    auto unlabeled = std::make_unique<OGRMultiLineString>();

    // 0.5 degrees is usually good enough for meteorological data, 1.0 degrees may
    // produce ~1 km errors
    const double linestep = 1.0;

    std::vector<double> x;
    std::vector<double> y;

    // Add vertical ticks only at top and bottom edges
    for (int lon = -180; lon <= 180; lon += g.step)
    {
      if (skip_value(lon, g.except))
        continue;

      // latlon coordinates
      for (int i = 0;; i++)
      {
        auto lat = -90.0 + i * linestep;
        if (lat > 90)
          break;
        x.push_back(lon);
        y.push_back(lat);
      }

      // CRS coordinates
      transformation.transform(x, y);

      // Find intersections with bbox

      std::vector<double> xcoord;
      std::vector<double> ycoord;

      OGRLineString* line = nullptr;
      for (auto i = 0UL; i < x.size() - 1; i++)
      {
        if (y[i] <= box.ymin() && y[i + 1] >= box.ymin())
          line = tick(y[i], y[i + 1], x[i], x[i + 1], box.ymin(), box, false, g.length);
        else if (y[i] <= box.ymax() && y[i + 1] >= box.ymax())
          line = tick(y[i + 1], y[i], x[i + 1], x[i], box.ymax(), box, false, g.length);
#if 0
        // We do not wish to draw meridians at the left/right edges
        else if (x[i] <= box.xmin() && x[i + 1] >= box.xmin())
          line = tick(x[i], x[i + 1], y[i], y[i + 1], box.xmin(), box, true, g.length);
        else if (x[i] <= box.xmax() && x[i + 1] >= box.xmax())
          line = tick(x[i + 1], x[i], y[i + 1], y[i], box.xmax(), box, true, g.length);
#endif

        if (line)
        {
          if (is_labeled(lon, g))
            labeled->addGeometryDirectly(line);
          else
            unlabeled->addGeometryDirectly(line);
        }
        line = nullptr;
      }
      x.clear();
      y.clear();
    }

    // Add horizontal ticks only at left and right edges
    for (int lat = -90 + g.step; lat < 90; lat += g.step)
    {
      if (skip_value(lat, g.except))
        continue;

      // latlon coordinates
      for (int i = 0;; i++)
      {
        auto lon = -180 + i * linestep;
        if (lon > 180)
          break;
        x.push_back(lon);
        y.push_back(lat);
      }

      // CRS coordinates
      transformation.transform(x, y);

      // Find intersections with bbox

      std::vector<double> xcoord;
      std::vector<double> ycoord;

      OGRLineString* line = nullptr;
      for (auto i = 0UL; i < x.size() - 1; i++)
      {
        if (x[i] <= box.xmin() && x[i + 1] >= box.xmin())
          line = tick(x[i], x[i + 1], y[i], y[i + 1], box.xmin(), box, true, g.length);
        else if (x[i] <= box.xmax() && x[i + 1] >= box.xmax())
          line = tick(x[i + 1], x[i], y[i + 1], y[i], box.xmax(), box, true, g.length);
#if 0
        // We do not wish to draw latitudes at the bottom/top edges
        else if (y[i] <= box.ymin() && y[i + 1] >= box.ymin())
          line = tick(y[i], y[i + 1], x[i], x[i + 1], box.ymin(), box, false, g.length);
        else if (y[i] <= box.ymax() && y[i + 1] >= box.ymax())
          line = tick(y[i + 1], y[i], x[i + 1], x[i], box.ymax(), box, false, g.length);
#endif

        if (line)
        {
          if (is_labeled(lat, g))
            labeled->addGeometryDirectly(line);
          else
            unlabeled->addGeometryDirectly(line);
        }
        line = nullptr;
      }
      x.clear();
      y.clear();
    }

    labeled->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());
    unlabeled->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());

    GraticuleLines glines;
    glines.labeled = std::move(labeled);
    glines.unlabeled = std::move(unlabeled);

    return glines;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule lines in image coordinates
 */
// ----------------------------------------------------------------------

GraticuleLines generate_lines(const Graticule& g,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box)
{
  try
  {
    GraticuleLines glines;

    if (g.layout == "grid")
      glines = generate_grid(g, crs, box);
    else if (g.layout == "ticks")
      glines = generate_ticks(g, crs, box);
    else
      throw Fmi::Exception(BCP, "Invalid graticule layout").addParameter("layout", g.layout);

    return glines;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Generate a single Graticule definition
 */
// ----------------------------------------------------------------------

Graticule remove_graticule(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "graticules settings must be JSON objects");

    Graticule g;
    auto& l = g.labels;  // shorthand

    JsonTools::remove_string(g.layout, theJson, "layout");
    JsonTools::remove_uint(g.step, theJson, "step");
    JsonTools::remove_uint(g.length, theJson, "length");
    auto json = JsonTools::remove(theJson, "attributes");
    g.attributes.init(json, theConfig);

    // By default labels are with the same step
    l.step = g.step;

    // exceptions to the given step
    json = JsonTools::remove(theJson, "except");
    JsonTools::extract_vector("except", g.except, json);

    // label rendering

    json = JsonTools::remove(theJson, "labels");

    if (!json.isNull())  // labels are not obligatory
    {
      if (!json.isObject())
        throw Fmi::Exception(BCP, "graticule labels setting must be a JSON object");

      JsonTools::remove_string(l.layout, json, "layout");
      JsonTools::remove_uint(l.step, json, "step");
      JsonTools::remove_bool(l.upright, json, "upright");
      JsonTools::remove_bool(l.degree_sign, json, "degree_sign");
      JsonTools::remove_bool(l.minus_sign, json, "minus_sign");
      JsonTools::remove_int(l.dx, json, "dx");
      JsonTools::remove_int(l.dy, json, "dy");

      auto j = JsonTools::remove(json, "attributes");
      l.attributes.init(j, theConfig);

      j = JsonTools::remove(json, "textattributes");
      l.textattributes.init(j, theConfig);
    }

    // Sanity checks
    if (valid_line_layouts.find(g.layout) == valid_line_layouts.end())
      throw Fmi::Exception(BCP, "graticule layout must be grid|ticks")
          .addParameter("layout", g.layout);

    if (g.step < 1)
      throw Fmi::Exception(BCP, "graticule step must be at least 1")
          .addParameter("step", Fmi::to_string(g.step));

    if (180 % g.step != 0)
      throw Fmi::Exception(BCP, "graticule step should divide 180 evenly")
          .addParameter("step", Fmi::to_string(g.step));

    if (g.layout == "ticks" && g.length < 1)
      throw Fmi::Exception(BCP, "graticule tick length must be at least 1");

    if (valid_label_layouts.find(l.layout) == valid_label_layouts.end())
      throw Fmi::Exception(BCP, "graticule labels layout must be none|edge|grid|center|edge_center")
          .addParameter("layout", l.layout);

    if (l.layout != "none")
    {
      if (l.step < 1)
        throw Fmi::Exception(BCP, "graticule label step must be at least 1")
            .addParameter("step", Fmi::to_string(l.step));

      if (180 % l.step != 0)
        throw Fmi::Exception(BCP, "graticule label step should divide 180 evenly")
            .addParameter("step", Fmi::to_string(l.step));
    }

    return g;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize graticule definitions
 */
// ----------------------------------------------------------------------

void GraticuleLayer::init(Json::Value& theJson,
                          const State& theState,
                          const Config& theConfig,
                          const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Gratitule-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("graticule");

    // Extract member values

    JsonTools::remove_string(mask, theJson, "mask");
    JsonTools::remove_string(mask_id, theJson, "mask_id");
    JsonTools::remove_double(precision, theJson, "precision");

    auto json = JsonTools::remove(theJson, "graticules");

    if (json.isNull())
      throw Fmi::Exception(BCP, "Graticule-layer must define at least one graticule");

    if (json.isObject())
      graticules.emplace_back(remove_graticule(json, theConfig));
    else if (json.isArray())
    {
      for (auto& gjson : json)
        graticules.emplace_back(remove_graticule(gjson, theConfig));
    }
    else
      throw Fmi::Exception(
          BCP,
          "graticules setting must be a single graticule definition or an array of definitions");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate data for content formatting
 *
 * We first generate the numbers for all graticules in the order they are listed.
 * Then if a mask is enabled, a mask layer will be generated based on the numbers.
 * The graticule lines are added last. The layer itself will be placed into
 * <g>...</g> with the main level attributes, and the numbers will be placed
 * into an inner block with label formatting attributes.
 */
// ----------------------------------------------------------------------

void GraticuleLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // Ignore formats not really meant for retrieving graticules

    if (theState.getType() == "topojson" || theState.getType() == "geojson")
      return;

    // Establish projection details

    auto q = getModel(theState);
    if (q)
      projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();
    const auto clipbox = getClipBox(box);

    // First generate the lines, and clip them to the image area

    std::vector<GraticuleLines> all_glines;
    for (const auto& graticule : graticules)
    {
      auto glines = generate_lines(graticule, crs, box);
      if (glines.labeled)
        glines.labeled.reset(Fmi::OGR::lineclip(*glines.labeled, clipbox));
      if (glines.unlabeled)
        glines.unlabeled.reset(Fmi::OGR::lineclip(*glines.unlabeled, clipbox));
      all_glines.emplace_back(std::move(glines));
    }

    // Handle CSS

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Clip if necessary
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Begin by generating a containing group

    std::string group_iri = (!qid.empty() ? qid : theState.makeQid("graticule-layer-"));
    if (!theState.addId(group_iri))
      throw Fmi::Exception(BCP, "Non-unique ID assigned to graticule")
          .addParameter("ID", group_iri);

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["attributes"]["id"] = group_iri;
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";
    group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theState.addAttributes(theGlobals, group_cdt, attributes);
    theLayersCdt.PushBack(group_cdt);

    // Add the lines
    for (auto i = 0UL; i < graticules.size(); i++)
    {
      const auto& graticule = graticules.at(i);
      const auto& glines = all_glines.at(i);

      if ((!glines.labeled || glines.labeled->IsEmpty() == 1) &&
          (!glines.unlabeled || glines.unlabeled->IsEmpty() == 1))
        continue;

      CTPP::CDT graticule_cdt(CTPP::CDT::HASH_VAL);

      // Store the path with unique QID
      std::string iri = qid + (qid.empty() ? "" : ".") + theState.makeQid("graticule");

      if (!theState.addId(iri))
        throw Fmi::Exception(BCP, "Non-unique ID assigned to graticule").addParameter("ID", iri);

      graticule_cdt["iri"] = iri;
      graticule_cdt["layertype"] = "graticule";

      auto geom = std::make_unique<OGRGeometryCollection>();
      if (glines.labeled && glines.labeled->IsEmpty() == 0)
        geom->addGeometry(glines.labeled.get());
      if (glines.unlabeled && glines.unlabeled->IsEmpty() == 0)
        geom->addGeometry(glines.unlabeled.get());

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

      if (!pointCoordinates.empty())
        graticule_cdt["data"] = pointCoordinates;

      theState.addPresentationAttributes(graticule_cdt, css, attributes, graticule.attributes);
      theGlobals["paths"][iri] = graticule_cdt;

      // Add the SVG use element
      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";
      theState.addAttributes(theGlobals, tag_cdt, graticule.attributes);
      tag_cdt["attributes"]["xlink:href"] = "#" + iri;
      graticule_cdt["tags"].PushBack(tag_cdt);

      theLayersCdt.PushBack(graticule_cdt);
    }

    // Add optional number groups to the containing group

    std::string labels_iri = (!mask_id.empty() ? mask_id : theState.makeQid("graticule-labels-"));
    if (!theState.addId(labels_iri))
      throw Fmi::Exception(BCP, "Non-unique ID assigned to graticule labels")
          .addParameter("ID", labels_iri);

    CTPP::CDT labels_cdt(CTPP::CDT::HASH_VAL);
    labels_cdt["attributes"]["id"] = labels_iri;
    labels_cdt["start"] = " <g";
    labels_cdt["end"] = "";
    theLayersCdt.PushBack(labels_cdt);

    for (auto i = 0UL; i < graticules.size(); i++)
    {
      const auto& graticule = graticules.at(i);

      auto labels = generate_labels(graticule, crs, box);

      if (!labels.empty())
      {
        std::string num_iri = theState.makeQid("graticule-numbers-");
        if (!theState.addId(num_iri))
          throw Fmi::Exception(BCP, "Non-unique ID assigned to graticule labels")
              .addParameter("ID", num_iri);

        CTPP::CDT num_cdt(CTPP::CDT::HASH_VAL);
        num_cdt["attributes"]["id"] = num_iri;
        num_cdt["start"] = "  <g";
        num_cdt["end"] = "";
        theState.addAttributes(theGlobals, num_cdt, graticule.labels.attributes);
        theLayersCdt.PushBack(num_cdt);

        for (const auto& label : labels)
        {
          CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
          text_cdt["start"] = "   <text";
          text_cdt["end"] = "</text>";
          text_cdt["cdata"] = label.text;

          auto xpos = lround(label.x + graticule.labels.dx);  // TODO: upright & angle
          auto ypos = lround(label.y + graticule.labels.dy);

          text_cdt["attributes"]["x"] = Fmi::to_string(xpos);
          text_cdt["attributes"]["y"] = Fmi::to_string(ypos);
          theState.addAttributes(theGlobals, text_cdt, graticule.labels.textattributes);

          theLayersCdt.PushBack(text_cdt);
        }
        // Close num_cdt
        theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n    </g>");
      }
    }

    // Close labels_cdt
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n   </g>");

    // Close group_cdt
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the graticule definitions
 */
// ----------------------------------------------------------------------

std::size_t GraticuleLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    for (const auto& g : graticules)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(g.layout));
      Fmi::hash_combine(hash, Fmi::hash_value(g.step));
      Fmi::hash_combine(hash, Fmi::hash_value(g.except));
      Fmi::hash_combine(hash, Fmi::hash_value(g.length));
      Fmi::hash_combine(hash, g.attributes.hash_value(theState));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.layout));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.step));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.upright));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.degree_sign));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.minus_sign));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.dx));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.dy));
      Fmi::hash_combine(hash, g.labels.attributes.hash_value(theState));
      Fmi::hash_combine(hash, g.labels.textattributes.hash_value(theState));
    }
    Fmi::hash_combine(hash, Fmi::hash_value(mask));
    Fmi::hash_combine(hash, Fmi::hash_value(mask_id));
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
