// ======================================================================
/*!
 * \brief The grid and terrain of one gridded producer
 */
// ======================================================================

#include "Area.h"
#include <fmt/format.h>
#include <gis/CoordinateTransformation.h>
#include <gis/SpatialReference.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <limits>
#include <utility>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Gridding
{
namespace
{
struct DatasetCloser
{
  void operator()(GDALDataset* ds) const { GDALClose(ds); }
};

using Dataset = std::unique_ptr<GDALDataset, DatasetCloser>;

// ----------------------------------------------------------------------
/*!
 * \brief Average a raster of sea fractions onto the grid
 *
 * Cells not covered by the raster are taken to be land.
 */
// ----------------------------------------------------------------------

Fmi::PointGridding::Field read_sea_fraction(const std::string& theFile,
                                            const Fmi::PointGridding::Grid& theGrid,
                                            const std::string& theWkt)
{
  Dataset src(static_cast<GDALDataset*>(GDALOpen(theFile.c_str(), GA_ReadOnly)));
  if (!src)
    throw Fmi::Exception(BCP, "Failed to open the sea fraction raster");

  const double x1 = theGrid.x0 + theGrid.dx * static_cast<double>(theGrid.nx);
  const double y1 = theGrid.y0 + theGrid.dx * static_cast<double>(theGrid.ny);
  std::vector<std::string> args{"-of",
                                "MEM",
                                "-t_srs",
                                theWkt,
                                "-te",
                                fmt::format("{:.17g}", theGrid.x0),
                                fmt::format("{:.17g}", theGrid.y0),
                                fmt::format("{:.17g}", x1),
                                fmt::format("{:.17g}", y1),
                                "-ts",
                                std::to_string(theGrid.nx),
                                std::to_string(theGrid.ny),
                                "-r",
                                "average",
                                "-ot",
                                "Float64",
                                "-dstnodata",
                                "nan"};
  std::vector<char*> argv;
  for (auto& arg : args)
    argv.push_back(arg.data());
  argv.push_back(nullptr);

  std::unique_ptr<GDALWarpAppOptions, decltype(&GDALWarpAppOptionsFree)> options(
      GDALWarpAppOptionsNew(argv.data(), nullptr), &GDALWarpAppOptionsFree);
  if (!options)
    throw Fmi::Exception(BCP, "Failed to set up the sea fraction warp");

  GDALDatasetH source = src.get();
  int error = 0;
  Dataset dst(static_cast<GDALDataset*>(GDALWarp("", nullptr, 1, &source, options.get(), &error)));
  if (!dst || error != 0)
    throw Fmi::Exception(BCP, "Failed to warp the sea fraction raster onto the grid");

  const auto nx = theGrid.nx;
  const auto ny = theGrid.ny;
  std::vector<double> rows(nx * ny);
  if (dst->GetRasterBand(1)->RasterIO(GF_Read,
                                      0,
                                      0,
                                      static_cast<int>(nx),
                                      static_cast<int>(ny),
                                      rows.data(),
                                      static_cast<int>(nx),
                                      static_cast<int>(ny),
                                      GDT_Float64,
                                      0,
                                      0) != CE_None)
    throw Fmi::Exception(BCP, "Failed to read the warped sea fraction");

  Fmi::PointGridding::Field field(theGrid, 0.0);
  for (std::size_t j = 0; j < ny; j++)
    for (std::size_t i = 0; i < nx; i++)
    {
      const double f = rows[(ny - 1 - j) * nx + i];
      if (std::isnan(f))
        continue;  // not covered: land
      if (f < -1e-6 || f > 1 + 1e-6)
        throw Fmi::Exception(BCP, "Sea fractions must be in the range 0-1")
            .addParameter("Value", fmt::format("{}", f));
      field(i, j) = std::clamp(f, 0.0, 1.0);
    }
  return field;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Read the terrain raster
 */
// ----------------------------------------------------------------------

Area::Area(const std::string& theDemFile,
           double theTpiSigma,
           const std::string& theSeaFractionFile,
           double theSeaSigma)
{
  try
  {
    GDALAllRegister();
    Dataset ds(static_cast<GDALDataset*>(GDALOpen(theDemFile.c_str(), GA_ReadOnly)));
    if (!ds)
      throw Fmi::Exception(BCP, "Failed to open the terrain raster");

    std::array<double, 6> gt{};
    if (ds->GetGeoTransform(gt.data()) != CE_None)
      throw Fmi::Exception(BCP, "The terrain raster has no geotransform");
    if (gt[2] != 0 || gt[4] != 0 || gt[5] >= 0)
      throw Fmi::Exception(BCP, "The terrain raster must be north-up");
    if (std::abs(gt[1] + gt[5]) > 1e-6 * gt[1])
      throw Fmi::Exception(BCP, "The terrain raster must have square cells");

    itsWkt = ds->GetProjectionRef();
    if (itsWkt.empty())
      throw Fmi::Exception(BCP, "The terrain raster has no coordinate system");

    const auto nx = static_cast<std::size_t>(ds->GetRasterXSize());
    const auto ny = static_cast<std::size_t>(ds->GetRasterYSize());
    Fmi::PointGridding::Grid grid;
    grid.dx = gt[1];
    grid.nx = nx;
    grid.ny = ny;
    grid.x0 = gt[0];
    grid.y0 = gt[3] + static_cast<double>(ny) * gt[5];

    auto* band = ds->GetRasterBand(1);
    int has_nodata = 0;
    const double nodata = band->GetNoDataValue(&has_nodata);

    std::vector<double> rows(nx * ny);
    if (band->RasterIO(GF_Read,
                       0,
                       0,
                       static_cast<int>(nx),
                       static_cast<int>(ny),
                       rows.data(),
                       static_cast<int>(nx),
                       static_cast<int>(ny),
                       GDT_Float64,
                       0,
                       0) != CE_None)
      throw Fmi::Exception(BCP, "Failed to read the terrain raster");

    // The raster starts from the north, the grid from the south. Missing heights are taken
    // to be sea, like bathymetry.
    std::vector<double> heights(nx * ny);
    for (std::size_t j = 0; j < ny; j++)
      for (std::size_t i = 0; i < nx; i++)
      {
        double h = rows[(ny - 1 - j) * nx + i];
        if (has_nodata != 0 && h == nodata)
          h = 0;
        heights[j * nx + i] = h;
      }

    itsTerrain =
        std::make_unique<Fmi::PointGridding::Terrain>(grid, std::move(heights), theTpiSigma);

    if (!theSeaFractionFile.empty())
    {
      try
      {
        itsTerrain->addCovariate(read_sea_fraction(theSeaFractionFile, grid, itsWkt), theSeaSigma);
      }
      catch (...)
      {
        throw Fmi::Exception::Trace(BCP, "Failed to read the sea fraction")
            .addParameter("Sea fraction", theSeaFractionFile);
      }
    }

    // WGS84 bounding box from points along the edges
    const int samples = 50;
    std::vector<double> x;
    std::vector<double> y;
    const double w = grid.dx * static_cast<double>(nx);
    const double h = grid.dx * static_cast<double>(ny);
    for (int k = 0; k <= samples; k++)
    {
      const double f = static_cast<double>(k) / samples;
      x.insert(x.end(), {grid.x0 + f * w, grid.x0 + f * w, grid.x0, grid.x0 + w});
      y.insert(y.end(), {grid.y0, grid.y0 + h, grid.y0 + f * h, grid.y0 + f * h});
    }
    Fmi::CoordinateTransformation transformation(Fmi::SpatialReference(itsWkt), "WGS84");
    transformation.transform(x, y);
    double minx = std::numeric_limits<double>::max();
    double maxx = -minx;
    double miny = minx;
    double maxy = -minx;
    for (std::size_t k = 0; k < x.size(); k++)
    {
      if (!std::isfinite(x[k]) || !std::isfinite(y[k]))
        continue;
      minx = std::min(minx, x[k]);
      maxx = std::max(maxx, x[k]);
      miny = std::min(miny, y[k]);
      maxy = std::max(maxy, y[k]);
    }
    if (minx > maxx)
      throw Fmi::Exception(BCP, "Failed to project the terrain raster to WGS84");
    itsBBox = {{"minx", minx}, {"miny", miny}, {"maxx", maxx}, {"maxy", maxy}};

    itsHash = Fmi::hash_value(theDemFile);
    Fmi::hash_combine(itsHash, Fmi::hash_value(theTpiSigma));
    if (!theSeaFractionFile.empty())
    {
      Fmi::hash_combine(itsHash, Fmi::hash_value(theSeaFractionFile));
      Fmi::hash_combine(itsHash, Fmi::hash_value(theSeaSigma));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to initialize gridding area")
        .addParameter("DEM", theDemFile);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WGS84 coordinates of the cell centres
 */
// ----------------------------------------------------------------------

const Fmi::CoordinateMatrix& Area::lonlats() const
{
  std::call_once(itsLonLatsFlag,
                 [this]()
                 {
                   const auto& g = grid();
                   auto m = std::make_unique<Fmi::CoordinateMatrix>(
                       g.nx,
                       g.ny,
                       g.x0 + 0.5 * g.dx,
                       g.y0 + 0.5 * g.dx,
                       g.x0 + (static_cast<double>(g.nx) - 0.5) * g.dx,
                       g.y0 + (static_cast<double>(g.ny) - 0.5) * g.dx);
                   Fmi::CoordinateTransformation transformation(Fmi::SpatialReference(itsWkt),
                                                                "WGS84");
                   m->transform(transformation);
                   itsLonLats = std::move(m);
                 });
  return *itsLonLats;
}

// ----------------------------------------------------------------------
/*!
 * \brief Project WGS84 coordinates to the grid coordinates
 */
// ----------------------------------------------------------------------

void Area::project(std::vector<double>& theX, std::vector<double>& theY) const
{
  // Points far outside the area are skipped, many projections cannot handle them. The
  // margin keeps stations just outside the area, they still affect values inside it.
  const double margin = 5;  // degrees
  const double minx = itsBBox.at("minx") - margin;
  const double maxx = itsBBox.at("maxx") + margin;
  const double miny = itsBBox.at("miny") - margin;
  const double maxy = itsBBox.at("maxy") + margin;

  // A transformation per call, the objects are not thread safe
  Fmi::CoordinateTransformation transformation("WGS84", Fmi::SpatialReference(itsWkt));
  for (std::size_t i = 0; i < theX.size(); i++)
    if (!(theX[i] >= minx && theX[i] <= maxx && theY[i] >= miny && theY[i] <= maxy) ||
        !transformation.transform(theX[i], theY[i]))
    {
      theX[i] = std::numeric_limits<double>::quiet_NaN();
      theY[i] = std::numeric_limits<double>::quiet_NaN();
    }
}

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
