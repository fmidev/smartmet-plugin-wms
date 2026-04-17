// ======================================================================
/*!
 * \brief Implementation of derived scalar fields.
 */
// ======================================================================

#include "ComputedFields.h"
#include "JsonTools.h"
#include <gis/CoordinateMatrix.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <newbase/NFmiGlobals.h>
#include <algorithm>
#include <cmath>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace ComputedFields
{

bool isTfpParameter(const std::string& parameter)
{
  return parameter == kTfpName;
}

TfpOptions parseTfpOptions(Json::Value& tfpJson)
{
  TfpOptions opts;
  if (tfpJson.isNull())
    return opts;
  if (!tfpJson.isObject())
    throw Fmi::Exception(BCP, "TFP settings must be a JSON object");
  JsonTools::remove_string(opts.field, tfpJson, "field");
  JsonTools::remove_int(opts.smoothing_passes, tfpJson, "smoothing_passes");
  JsonTools::remove_double(opts.min_gradient, tfpJson, "min_gradient");
  JsonTools::remove_double(opts.scale, tfpJson, "scale");
  if (opts.smoothing_passes < 0)
    throw Fmi::Exception(BCP, "tfp.smoothing_passes must be nonnegative");
  if (opts.min_gradient < 0)
    throw Fmi::Exception(BCP, "tfp.min_gradient must be nonnegative");
  return opts;
}

void hashTfpOptions(std::size_t& seed, const TfpOptions& opts)
{
  Fmi::hash_combine(seed, Fmi::hash_value(opts.field));
  Fmi::hash_combine(seed, Fmi::hash_value(opts.smoothing_passes));
  Fmi::hash_combine(seed, Fmi::hash_value(opts.min_gradient));
  Fmi::hash_combine(seed, Fmi::hash_value(opts.scale));
}

NFmiDataMatrix<float> smoothScalar(const NFmiDataMatrix<float>& field, int passes)
{
  if (passes <= 0)
    return field;
  const std::size_t W = field.NX();
  const std::size_t H = field.NY();
  NFmiDataMatrix<float> a = field;
  NFmiDataMatrix<float> b(W, H, kFloatMissing);
  for (int p = 0; p < passes; ++p)
  {
    for (std::size_t j = 0; j < H; ++j)
    {
      for (std::size_t i = 0; i < W; ++i)
      {
        const int iw[3] = {1, 2, 1};
        const int jw[3] = {1, 2, 1};
        double sum = 0.0;
        double wsum = 0.0;
        for (int dj = -1; dj <= 1; ++dj)
        {
          const std::ptrdiff_t jj = static_cast<std::ptrdiff_t>(j) + dj;
          if (jj < 0 || jj >= static_cast<std::ptrdiff_t>(H))
            continue;
          for (int di = -1; di <= 1; ++di)
          {
            const std::ptrdiff_t ii = static_cast<std::ptrdiff_t>(i) + di;
            if (ii < 0 || ii >= static_cast<std::ptrdiff_t>(W))
              continue;
            const float v = a[ii][jj];
            if (v == kFloatMissing)
              continue;
            const double w = iw[di + 1] * jw[dj + 1];
            sum += w * v;
            wsum += w;
          }
        }
        b[i][j] = (wsum > 0.0) ? static_cast<float>(sum / wsum) : kFloatMissing;
      }
    }
    std::swap(a, b);
  }
  return a;
}

namespace
{
// Differentiate a scalar matrix at cell (i, j) in x and y with centered
// differences where possible, falling back to one-sided (forward or
// backward) differences at grid edges so the output matches the full
// input shape.
//
// Returns true if both partial derivatives were computable; false if any
// required neighbour was missing or the grid spacing collapsed to zero.
bool gradientAt(const NFmiDataMatrix<float>& f,
                const Fmi::CoordinateMatrix& coords,
                std::size_t i,
                std::size_t j,
                double& gx,
                double& gy)
{
  constexpr double kDegToM = 111319.5;
  constexpr double kPi = M_PI;

  const std::size_t W = f.NX();
  const std::size_t H = f.NY();
  if (W < 2 || H < 2)
    return false;

  // Centered difference where possible; at the edges fall back to a
  // one-sided pair. Both pairs span the same number of cells, so we just
  // divide (fR - fL) by the actual metric distance between coords(iL)
  // and coords(iR) without worrying about whether it's 1 cell or 2.
  const std::size_t iL = (i == 0) ? 0 : i - 1;
  const std::size_t iR = (i + 1 >= W) ? W - 1 : i + 1;
  const std::size_t jB = (j == 0) ? 0 : j - 1;
  const std::size_t jT = (j + 1 >= H) ? H - 1 : j + 1;
  if (iL == iR || jB == jT)
    return false;

  const float fL = f[iL][j];
  const float fR = f[iR][j];
  const float fB = f[i][jB];
  const float fT = f[i][jT];
  if (fL == kFloatMissing || fR == kFloatMissing || fB == kFloatMissing ||
      fT == kFloatMissing)
    return false;

  const double lat = coords.y(i, j);
  const double dx_m = (coords.x(iR, j) - coords.x(iL, j)) * std::cos(lat * kPi / 180.0) * kDegToM;
  const double dy_m = (coords.y(i, jT) - coords.y(i, jB)) * kDegToM;
  if (std::abs(dx_m) < 1.0 || std::abs(dy_m) < 1.0)
    return false;

  gx = (fR - fL) / dx_m;
  gy = (fT - fB) / dy_m;
  return true;
}
}  // namespace

NFmiDataMatrix<float> computeTFP(const NFmiDataMatrix<float>& field,
                                 const Fmi::CoordinateMatrix& coords,
                                 double min_gradient)
{
  const std::size_t W = field.NX();
  const std::size_t H = field.NY();

  // First pass: ∂F/∂x, ∂F/∂y, |∇F| over the full grid, using one-sided
  // differences at edges so the TFP result has the same coverage as the
  // input rather than a missing border.
  NFmiDataMatrix<float> dFdx(W, H, kFloatMissing);
  NFmiDataMatrix<float> dFdy(W, H, kFloatMissing);
  NFmiDataMatrix<float> M(W, H, kFloatMissing);

  for (std::size_t j = 0; j < H; ++j)
  {
    for (std::size_t i = 0; i < W; ++i)
    {
      double gx, gy;
      if (!gradientAt(field, coords, i, j, gx, gy))
        continue;
      dFdx[i][j] = static_cast<float>(gx);
      dFdy[i][j] = static_cast<float>(gy);
      M[i][j] = static_cast<float>(std::sqrt(gx * gx + gy * gy));
    }
  }

  // Second pass: TFP = -∇(|∇F|) · ∇F / |∇F|, again full-grid with
  // one-sided differences at edges.
  NFmiDataMatrix<float> tfp(W, H, kFloatMissing);

  for (std::size_t j = 0; j < H; ++j)
  {
    for (std::size_t i = 0; i < W; ++i)
    {
      const float m = M[i][j];
      if (m == kFloatMissing)
        continue;
      if (min_gradient > 0.0 && m < static_cast<float>(min_gradient))
        continue;

      double dMdx, dMdy;
      if (!gradientAt(M, coords, i, j, dMdx, dMdy))
        continue;

      tfp[i][j] = static_cast<float>(-(dMdx * dFdx[i][j] + dMdy * dFdy[i][j]) / m);
    }
  }

  return tfp;
}

}  // namespace ComputedFields
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
