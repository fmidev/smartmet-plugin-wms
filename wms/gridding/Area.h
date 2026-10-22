// ======================================================================
/*!
 * \brief The grid and terrain of one gridded producer
 */
// ======================================================================

#pragma once

#include <gis/PointGridding.h>
#include <gis/SpatialReference.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Gridding
{
class Area
{
 public:
  // Read the terrain from a north-up raster with square cells. The raster defines the grid
  // and its coordinate system.
  Area(const std::string& theDemFile, double theTpiSigma);

  const Fmi::PointGridding::Terrain& terrain() const { return *itsTerrain; }
  const Fmi::PointGridding::Grid& grid() const { return itsTerrain->grid(); }
  const std::string& crsWkt() const { return itsWkt; }

  // Project WGS84 longitudes and latitudes to the grid coordinates in place. Points which
  // cannot be projected become NaN.
  void project(std::vector<double>& theX, std::vector<double>& theY) const;

  // WGS84 bounding box of the grid with keys minx, miny, maxx, maxy
  const std::map<std::string, double>& lonlatBBox() const { return itsBBox; }

  std::size_t hashValue() const { return itsHash; }

 private:
  std::unique_ptr<Fmi::PointGridding::Terrain> itsTerrain;
  std::string itsWkt;
  std::map<std::string, double> itsBBox;
  std::size_t itsHash = 0;
};

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
