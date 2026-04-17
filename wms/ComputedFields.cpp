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

NFmiDataMatrix<float> computeTFP(const NFmiDataMatrix<float>& field,
                                 const Fmi::CoordinateMatrix& coords,
                                 double min_gradient)
{
  const std::size_t W = field.NX();
  const std::size_t H = field.NY();
  constexpr double kDegToM = 111319.5;  // metres per degree latitude
  constexpr double kPi = M_PI;

  // First pass: ∂F/∂x, ∂F/∂y, |∇F|
  NFmiDataMatrix<float> dFdx(W, H, kFloatMissing);
  NFmiDataMatrix<float> dFdy(W, H, kFloatMissing);
  NFmiDataMatrix<float> M(W, H, kFloatMissing);

  for (std::size_t j = 1; j + 1 < H; ++j)
  {
    for (std::size_t i = 1; i + 1 < W; ++i)
    {
      const float fL = field[i - 1][j];
      const float fR = field[i + 1][j];
      const float fB = field[i][j - 1];
      const float fT = field[i][j + 1];

      if (fL == kFloatMissing || fR == kFloatMissing || fB == kFloatMissing ||
          fT == kFloatMissing)
        continue;

      const double lat = coords.y(i, j);
      const double lonL = coords.x(i - 1, j);
      const double lonR = coords.x(i + 1, j);
      const double latB = coords.y(i, j - 1);
      const double latT = coords.y(i, j + 1);

      const double dx_m = (lonR - lonL) * 0.5 * std::cos(lat * kPi / 180.0) * kDegToM;
      const double dy_m = (latT - latB) * 0.5 * kDegToM;

      if (std::abs(dx_m) < 1.0 || std::abs(dy_m) < 1.0)
        continue;

      const double gx = (fR - fL) / (2.0 * dx_m);
      const double gy = (fT - fB) / (2.0 * dy_m);
      const double m = std::sqrt(gx * gx + gy * gy);

      dFdx[i][j] = static_cast<float>(gx);
      dFdy[i][j] = static_cast<float>(gy);
      M[i][j] = static_cast<float>(m);
    }
  }

  // Second pass: TFP = -(∂M/∂x * ∂F/∂x + ∂M/∂y * ∂F/∂y) / M
  NFmiDataMatrix<float> tfp(W, H, kFloatMissing);

  for (std::size_t j = 1; j + 1 < H; ++j)
  {
    for (std::size_t i = 1; i + 1 < W; ++i)
    {
      const float m = M[i][j];
      if (m == kFloatMissing)
        continue;
      if (min_gradient > 0.0 && m < static_cast<float>(min_gradient))
        continue;

      const float mL = M[i - 1][j];
      const float mR = M[i + 1][j];
      const float mB = M[i][j - 1];
      const float mT = M[i][j + 1];

      if (mL == kFloatMissing || mR == kFloatMissing || mB == kFloatMissing ||
          mT == kFloatMissing)
        continue;

      const double lat = coords.y(i, j);
      const double lonL = coords.x(i - 1, j);
      const double lonR = coords.x(i + 1, j);
      const double latB = coords.y(i, j - 1);
      const double latT = coords.y(i, j + 1);

      const double dx_m = (lonR - lonL) * 0.5 * std::cos(lat * kPi / 180.0) * kDegToM;
      const double dy_m = (latT - latB) * 0.5 * kDegToM;

      if (std::abs(dx_m) < 1.0 || std::abs(dy_m) < 1.0)
        continue;

      const double dMdx = (mR - mL) / (2.0 * dx_m);
      const double dMdy = (mT - mB) / (2.0 * dy_m);
      tfp[i][j] = static_cast<float>(-(dMdx * dFdx[i][j] + dMdy * dFdy[i][j]) / m);
    }
  }

  return tfp;
}

}  // namespace ComputedFields
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
