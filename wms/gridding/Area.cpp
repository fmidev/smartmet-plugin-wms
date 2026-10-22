// ======================================================================
/*!
 * \brief The grid and terrain of one gridded producer
 */
// ======================================================================

#include "Area.h"
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <algorithm>
#include <cmath>
#include <gdal_priv.h>
#include <limits>

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
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Read the terrain raster
 */
// ----------------------------------------------------------------------

Area::Area(const std::string& theDemFile, double theTpiSigma)
{
  try
  {
    GDALAllRegister();
    std::unique_ptr<GDALDataset, DatasetCloser> ds(
        static_cast<GDALDataset*>(GDALOpen(theDemFile.c_str(), GA_ReadOnly)));
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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to initialize gridding area")
        .addParameter("DEM", theDemFile);
  }
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
