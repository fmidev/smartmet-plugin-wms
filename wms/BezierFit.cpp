// ======================================================================
/*!
 * \brief Bezier curve fitting based on Kurbo's moment-matching algorithm.
 *
 * Port of Raph Levien's algorithm from https://github.com/linebender/kurbo
 * Original license: Apache-2.0 OR MIT
 *
 * The algorithm finds cubic Bezier curves whose signed area and first
 * moment match the source curve, by solving a quartic polynomial derived
 * from the tangent angles and moment integrals.
 */
// ======================================================================

#include "BezierFit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

namespace Fmi
{
namespace BezierFit
{
namespace
{

// ======================================================================
// Gauss-Legendre 16-point quadrature coefficients (weight, node)
// ======================================================================

constexpr std::array<std::pair<double, double>, 16> GL16 = {{
    {0.1894506104550685, -0.0950125098376374},
    {0.1894506104550685, 0.0950125098376374},
    {0.1826034150449236, -0.2816035507792589},
    {0.1826034150449236, 0.2816035507792589},
    {0.1691565193950025, -0.4580167776572274},
    {0.1691565193950025, 0.4580167776572274},
    {0.1495959888165767, -0.6178762444026438},
    {0.1495959888165767, 0.6178762444026438},
    {0.1246289712555339, -0.7554044083550030},
    {0.1246289712555339, 0.7554044083550030},
    {0.0951585116824928, -0.8656312023878318},
    {0.0951585116824928, 0.8656312023878318},
    {0.0622535239386479, -0.9445750230732326},
    {0.0622535239386479, 0.9445750230732326},
    {0.0271524594117541, -0.9894009349916499},
    {0.0271524594117541, 0.9894009349916499},
}};

// ======================================================================
// Polynomial solvers (ported from kurbo/src/common.rs)
// ======================================================================

using SmallVec2 = std::array<double, 2>;
using SmallVec3 = std::array<double, 3>;
using SmallVec4 = std::array<double, 4>;

struct Roots
{
  std::array<double, 4> vals{};
  int count = 0;
  void push(double v) { vals[count++] = v; }
};

double eps_rel(double raw, double a)
{
  if (a == 0.0)
    return std::abs(raw);
  return std::abs((raw - a) / a);
}

Roots solve_quadratic(double c0, double c1, double c2)
{
  Roots result;
  double sc0 = c0 / c2;
  double sc1 = c1 / c2;
  if (!std::isfinite(sc0) || !std::isfinite(sc1))
  {
    double root = -c0 / c1;
    if (std::isfinite(root))
      result.push(root);
    else if (c0 == 0.0 && c1 == 0.0)
      result.push(0.0);
    return result;
  }
  double arg = sc1 * sc1 - 4.0 * sc0;
  double root1;
  if (!std::isfinite(arg))
  {
    root1 = -sc1;
  }
  else
  {
    if (arg < 0.0)
      return result;
    if (arg == 0.0)
    {
      result.push(-0.5 * sc1);
      return result;
    }
    root1 = -0.5 * (sc1 + std::copysign(std::sqrt(arg), sc1));
  }
  double root2 = sc0 / root1;
  if (std::isfinite(root2))
  {
    if (root2 > root1)
    {
      result.push(root1);
      result.push(root2);
    }
    else
    {
      result.push(root2);
      result.push(root1);
    }
  }
  else
  {
    result.push(root1);
  }
  return result;
}

Roots solve_cubic(double c0, double c1, double c2, double c3)
{
  Roots result;
  constexpr double ONE_THIRD = 1.0 / 3.0;
  double c3_recip = 1.0 / c3;
  double scaled_c2 = c2 * (ONE_THIRD * c3_recip);
  double scaled_c1 = c1 * (ONE_THIRD * c3_recip);
  double scaled_c0 = c0 * c3_recip;

  if (!std::isfinite(scaled_c0) || !std::isfinite(scaled_c1) || !std::isfinite(scaled_c2))
  {
    auto qr = solve_quadratic(c0, c1, c2);
    for (int i = 0; i < qr.count; i++)
      result.push(qr.vals[i]);
    return result;
  }
  double d0 = std::fma(-scaled_c2, scaled_c2, scaled_c1);
  double d1 = std::fma(-scaled_c1, scaled_c2, scaled_c0);
  double d2 = scaled_c2 * scaled_c0 - scaled_c1 * scaled_c1;
  double d = 4.0 * d0 * d2 - d1 * d1;
  double de = std::fma(-2.0 * scaled_c2, d0, d1);

  if (d < 0.0)
  {
    double sq = std::sqrt(-0.25 * d);
    double r = -0.5 * de;
    double t1 = std::cbrt(r + sq) + std::cbrt(r - sq);
    result.push(t1 - scaled_c2);
  }
  else if (d == 0.0)
  {
    double t1 = std::copysign(std::sqrt(-d0), de);
    result.push(t1 - scaled_c2);
    result.push(-2.0 * t1 - scaled_c2);
  }
  else
  {
    double th = std::atan2(std::sqrt(d), -de) * ONE_THIRD;
    double th_sin = std::sin(th);
    double th_cos = std::cos(th);
    double r0 = th_cos;
    double ss3 = th_sin * std::sqrt(3.0);
    double r1 = 0.5 * (-th_cos + ss3);
    double r2 = 0.5 * (-th_cos - ss3);
    double t = 2.0 * std::sqrt(-d0);
    result.push(std::fma(t, r0, -scaled_c2));
    result.push(std::fma(t, r1, -scaled_c2));
    result.push(std::fma(t, r2, -scaled_c2));
  }
  return result;
}

// Depressed cubic dominant root (for quartic factoring)
double depressed_cubic_dominant(double g, double h)
{
  double q = (-1.0 / 3.0) * g;
  double r = 0.5 * h;
  double phi_0;

  bool use_k = (std::abs(q) >= 1e102 || std::abs(r) >= 1e154);
  double k = 0;
  bool k_neg = false;
  if (use_k)
  {
    if (std::abs(q) < std::abs(r))
      k = 1.0 - q * (q / r) * (q / r);
    else
      k = std::copysign(1.0, q) * ((r / q) * (r / q) / q - 1.0);
    k_neg = (k < 0.0);
  }

  if (use_k && r == 0.0)
  {
    phi_0 = (g > 0.0) ? 0.0 : std::sqrt(-g);
  }
  else if ((use_k && k_neg) || (!use_k && r * r < q * q * q))
  {
    double t;
    if (use_k)
      t = r / q / std::sqrt(q);
    else
      t = r / std::sqrt(q * q * q);
    phi_0 = -2.0 * std::sqrt(q) * std::copysign(std::cos(std::acos(std::abs(t)) / 3.0), t);
  }
  else
  {
    double a_val;
    if (use_k)
    {
      if (std::abs(q) < std::abs(r))
        a_val = -r * (1.0 + std::sqrt(k));
      else
        a_val = -r - std::copysign(std::sqrt(std::abs(q)) * q * std::sqrt(k), r);
    }
    else
    {
      a_val = -r - std::copysign(std::sqrt(r * r - q * q * q), r);
    }
    a_val = std::cbrt(a_val);
    double b_val = (a_val == 0.0) ? 0.0 : q / a_val;
    phi_0 = a_val + b_val;
  }

  // Newton refinement
  double x = phi_0;
  double f = (x * x + g) * x + h;
  constexpr double EPS_M = 2.22045e-16;
  double threshold = EPS_M * std::max({std::abs(x * x * x), std::abs(g * x), std::abs(h)});
  if (std::abs(f) < threshold)
    return x;

  for (int i = 0; i < 8; i++)
  {
    double delt_f = 3.0 * x * x + g;
    if (delt_f == 0.0)
      break;
    double new_x = x - f / delt_f;
    double new_f = (new_x * new_x + g) * new_x + h;
    if (new_f == 0.0)
      return new_x;
    if (std::abs(new_f) >= std::abs(f))
      break;
    x = new_x;
    f = new_f;
  }
  return x;
}

// Factor quartic into two quadratics: (x^2 + a1*x + b1)(x^2 + a2*x + b2)
// Returns the two quadratic coefficient pairs, or nullopt on failure.
struct QuadPair
{
  double a1, b1, a2, b2;
};

std::optional<QuadPair> factor_quartic_inner(double a, double b, double c, double d, bool rescale)
{
  auto calc_eps_q = [&](double a1, double b1, double a2, double b2)
  {
    double eps_a = eps_rel(a1 + a2, a);
    double eps_b = eps_rel(b1 + a1 * a2 + b2, b);
    double eps_c = eps_rel(b1 * a2 + a1 * b2, c);
    return eps_a + eps_b + eps_c;
  };
  auto calc_eps_t = [&](double a1, double b1, double a2, double b2)
  { return calc_eps_q(a1, b1, a2, b2) + eps_rel(b1 * b2, d); };

  double disc = 9.0 * a * a - 24.0 * b;
  double s;
  if (disc >= 0.0)
    s = -2.0 * b / (3.0 * a + std::copysign(std::sqrt(disc), a));
  else
    s = -0.25 * a;

  double a_prime = a + 4.0 * s;
  double b_prime = b + 3.0 * s * (a + 2.0 * s);
  double c_prime = c + s * (2.0 * b + s * (3.0 * a + 4.0 * s));
  double d_prime = d + s * (c + s * (b + s * (a + s)));

  double g_prime, h_prime;
  constexpr double K_C = 3.49e102;
  if (rescale)
  {
    double aps = a_prime / K_C;
    double bps = b_prime / K_C;
    double cps = c_prime / K_C;
    double dps = d_prime / K_C;
    g_prime = aps * cps - (4.0 / K_C) * dps - (1.0 / 3.0) * bps * bps;
    h_prime = (aps * cps + (8.0 / K_C) * dps - (2.0 / 9.0) * bps * bps) * (1.0 / 3.0) * bps -
              cps * (cps / K_C) - aps * aps * dps;
  }
  else
  {
    g_prime = a_prime * c_prime - 4.0 * d_prime - (1.0 / 3.0) * b_prime * b_prime;
    h_prime = (a_prime * c_prime + 8.0 * d_prime - (2.0 / 9.0) * b_prime * b_prime) *
                  (1.0 / 3.0) * b_prime -
              c_prime * c_prime - a_prime * a_prime * d_prime;
  }
  if (!std::isfinite(g_prime) || !std::isfinite(h_prime))
    return std::nullopt;

  double phi = depressed_cubic_dominant(g_prime, h_prime);
  if (rescale)
    phi *= K_C;

  double l_1 = a * 0.5;
  double l_3 = (1.0 / 6.0) * b + 0.5 * phi;
  double delt_2 = c - a * l_3;
  double d_2_cand_1 = (2.0 / 3.0) * b - phi - l_1 * l_1;
  double l_2_cand_1 = (d_2_cand_1 != 0.0) ? 0.5 * delt_2 / d_2_cand_1 : 0.0;
  double l_2_cand_2 = (delt_2 != 0.0) ? 2.0 * (d - l_3 * l_3) / delt_2 : 0.0;
  double d_2_cand_2 = (l_2_cand_2 != 0.0) ? 0.5 * delt_2 / l_2_cand_2 : 0.0;

  double d_2_best = 0;
  double l_2_best = 0;
  double eps_l_best = 0;
  struct Cand
  {
    double d2, l2;
  };
  Cand cands[3] = {
      {d_2_cand_1, l_2_cand_1}, {d_2_cand_2, l_2_cand_2}, {d_2_cand_1, l_2_cand_2}};

  for (int i = 0; i < 3; i++)
  {
    double d2 = cands[i].d2;
    double l2 = cands[i].l2;
    double eps_0 = eps_rel(d2 + l_1 * l_1 + 2.0 * l_3, b);
    double eps_1 = eps_rel(2.0 * (d2 * l2 + l_1 * l_3), c);
    double eps_2 = eps_rel(d2 * l2 * l2 + l_3 * l_3, d);
    double eps_l = eps_0 + eps_1 + eps_2;
    if (i == 0 || eps_l < eps_l_best)
    {
      d_2_best = d2;
      l_2_best = l2;
      eps_l_best = eps_l;
    }
  }

  double d_2 = d_2_best;
  double l_2 = l_2_best;

  double alpha_1, beta_1, alpha_2, beta_2;

  if (d_2 < 0.0)
  {
    double sq = std::sqrt(-d_2);
    alpha_1 = l_1 + sq;
    beta_1 = l_3 + sq * l_2;
    alpha_2 = l_1 - sq;
    beta_2 = l_3 - sq * l_2;

    if (std::abs(beta_2) < std::abs(beta_1))
      beta_2 = d / beta_1;
    else if (std::abs(beta_2) > std::abs(beta_1))
      beta_1 = d / beta_2;

    if (std::abs(alpha_1) != std::abs(alpha_2))
    {
      struct ACand
      {
        double a1, a2;
      };
      ACand acands[3];

      if (std::abs(alpha_1) < std::abs(alpha_2))
      {
        double a1_c1 = (beta_2 != 0.0) ? (c - beta_1 * alpha_2) / beta_2 : alpha_1;
        double a1_c2 = (alpha_2 != 0.0) ? (b - beta_2 - beta_1) / alpha_2 : alpha_1;
        double a1_c3 = a - alpha_2;
        acands[0] = {a1_c3, alpha_2};
        acands[1] = {a1_c1, alpha_2};
        acands[2] = {a1_c2, alpha_2};
      }
      else
      {
        double a2_c1 = (beta_1 != 0.0) ? (c - alpha_1 * beta_2) / beta_1 : alpha_2;
        double a2_c2 = (alpha_1 != 0.0) ? (b - beta_2 - beta_1) / alpha_1 : alpha_2;
        double a2_c3 = a - alpha_1;
        acands[0] = {alpha_1, a2_c3};
        acands[1] = {alpha_1, a2_c1};
        acands[2] = {alpha_1, a2_c2};
      }

      double eps_q_best = 0;
      for (int i = 0; i < 3; i++)
      {
        if (std::isfinite(acands[i].a1) && std::isfinite(acands[i].a2))
        {
          double eps_q = calc_eps_q(acands[i].a1, beta_1, acands[i].a2, beta_2);
          if (i == 0 || eps_q < eps_q_best)
          {
            alpha_1 = acands[i].a1;
            alpha_2 = acands[i].a2;
            eps_q_best = eps_q;
          }
        }
      }
    }
  }
  else if (d_2 == 0.0)
  {
    double d_3 = d - l_3 * l_3;
    alpha_1 = l_1;
    beta_1 = l_3 + std::sqrt(std::max(0.0, -d_3));
    alpha_2 = l_1;
    beta_2 = l_3 - std::sqrt(std::max(0.0, -d_3));

    if (std::abs(beta_1) > std::abs(beta_2))
      beta_2 = d / beta_1;
    else if (std::abs(beta_2) > std::abs(beta_1))
      beta_1 = d / beta_2;
  }
  else
  {
    return std::nullopt;
  }

  // Newton refinement
  double eps_t = calc_eps_t(alpha_1, beta_1, alpha_2, beta_2);
  for (int iter = 0; iter < 8; iter++)
  {
    if (eps_t == 0.0)
      break;

    double f_0 = beta_1 * beta_2 - d;
    double f_1 = beta_1 * alpha_2 + alpha_1 * beta_2 - c;
    double f_2 = beta_1 + alpha_1 * alpha_2 + beta_2 - b;
    double f_3 = alpha_1 + alpha_2 - a;

    double c_1 = alpha_1 - alpha_2;
    double det_j = beta_1 * beta_1 - beta_1 * (alpha_2 * c_1 + 2.0 * beta_2) +
                   beta_2 * (alpha_1 * c_1 + beta_2);
    if (det_j == 0.0)
      break;

    double inv = 1.0 / det_j;
    double c_2 = beta_2 - beta_1;
    double c_3 = beta_1 * alpha_2 - alpha_1 * beta_2;

    double dz_0 =
        c_1 * f_0 + c_2 * f_1 + c_3 * f_2 - (beta_1 * c_2 + alpha_1 * c_3) * f_3;
    double dz_1 =
        (alpha_1 * c_1 + c_2) * f_0 - beta_1 * c_1 * f_1 - beta_1 * c_2 * f_2 - beta_1 * c_3 * f_3;
    double dz_2 =
        -c_1 * f_0 - c_2 * f_1 - c_3 * f_2 + (alpha_2 * c_3 + beta_2 * c_2) * f_3;
    double dz_3 =
        -(alpha_2 * c_1 + c_2) * f_0 + beta_2 * c_1 * f_1 + beta_2 * c_2 * f_2 + beta_2 * c_3 * f_3;

    double a1 = alpha_1 - inv * dz_0;
    double b1 = beta_1 - inv * dz_1;
    double a2 = alpha_2 - inv * dz_2;
    double b2 = beta_2 - inv * dz_3;

    double new_eps_t = calc_eps_t(a1, b1, a2, b2);
    if (new_eps_t < eps_t)
    {
      alpha_1 = a1;
      beta_1 = b1;
      alpha_2 = a2;
      beta_2 = b2;
      eps_t = new_eps_t;
    }
    else
    {
      break;
    }
  }

  return QuadPair{alpha_1, beta_1, alpha_2, beta_2};
}

// ITP root-finding method (Interpolate-Truncate-Project)
// Returns (a, b) interval bracketing the root, or (cusp, cusp) if cusp detected.
struct ItpResult
{
  double a;
  double b;
  bool cusp_found = false;
};

ItpResult solve_itp(const std::function<std::pair<double, bool>(double)>& f,
                    double a,
                    double b,
                    double epsilon,
                    int n0,
                    double k1,
                    double ya,
                    double yb)
{
  double n1_2_raw = std::log2((b - a) / epsilon);
  int n1_2 = static_cast<int>(std::max(0.0, std::ceil(n1_2_raw - 1.0)));
  int nmax = n0 + n1_2;
  double scaled_epsilon = epsilon * static_cast<double>(1ULL << std::min(nmax, 62));

  while (b - a > 2.0 * epsilon)
  {
    double x1_2 = 0.5 * (a + b);
    double r = scaled_epsilon - 0.5 * (b - a);
    double xf = (yb * a - ya * b) / (yb - ya);
    double sigma = x1_2 - xf;
    double delta = k1 * (b - a) * (b - a);
    double xt;
    if (delta <= std::abs(x1_2 - xf))
      xt = xf + std::copysign(delta, sigma);
    else
      xt = x1_2;
    double xitp;
    if (std::abs(xt - x1_2) <= r)
      xitp = xt;
    else
      xitp = x1_2 - std::copysign(r, sigma);

    auto [yitp, is_cusp] = f(xitp);
    if (is_cusp)
      return {xitp, xitp, true};

    if (yitp > 0.0)
    {
      b = xitp;
      yb = yitp;
    }
    else if (yitp < 0.0)
    {
      a = xitp;
      ya = yitp;
    }
    else
    {
      return {xitp, xitp, false};
    }
    scaled_epsilon *= 0.5;
  }
  return {a, b, false};
}

// ======================================================================
// CubicBez operations
// ======================================================================

// Nearest point on a line segment to a given point
double line_nearest_dist_sq(const Point& a, const Point& b, const Point& p)
{
  Vec2 d = b - a;
  double len2 = d.hypot2();
  if (len2 == 0.0)
    return distance_sq(a, p);
  double t = std::clamp(((p - a).dot(d)) / len2, 0.0, 1.0);
  Point proj = a + d * t;
  return distance_sq(p, proj);
}

// ======================================================================
// Curve distance measurement (ported from CurveDist in fit.rs)
// ======================================================================

constexpr int N_SAMPLE = 20;
constexpr double SPICY_THRESH = 0.2;

struct CurveFitSample
{
  Point p;
  Vec2 tangent;
};

// Abstract source curve interface (like ParamCurveFit trait)
struct SourceCurve
{
  virtual ~SourceCurve() = default;
  virtual CurveFitSample sample_pt_tangent(double t, double sign) const = 0;
  virtual std::pair<Point, Vec2> sample_pt_deriv(double t) const = 0;

  // Default: Gauss-Legendre quadrature using Green's theorem
  virtual std::tuple<double, double, double> moment_integrals(double t0, double t1) const
  {
    double tm = 0.5 * (t0 + t1);
    double dt = 0.5 * (t1 - t0);
    double area = 0;
    double mx = 0;
    double my = 0;
    for (const auto& [wi, xi] : GL16)
    {
      double t = tm + xi * dt;
      auto [p, d] = sample_pt_deriv(t);
      double a = wi * d.x * p.y;
      area += a;
      mx += p.x * a;
      my += p.y * a;
    }
    return {area * dt, mx * dt, my * dt};
  }

  // Override to detect cusps within a range
  virtual std::optional<double> break_cusp(double /*t0*/, double /*t1*/) const
  {
    return std::nullopt;
  }
};

// Intersect a ray orthogonal to tangent with a cubic Bezier.
// Returns t values on the cubic where the ray intersects.
Roots intersect_ray(const CurveFitSample& sample, const CubicBez& c)
{
  Vec2 p1 = 3.0 * (c.p1 - c.p0);
  Vec2 p2 = 3.0 * Vec2{c.p2.x, c.p2.y} - 6.0 * Vec2{c.p1.x, c.p1.y} +
            3.0 * Vec2{c.p0.x, c.p0.y};
  Vec2 p3 = (c.p3 - c.p0) - 3.0 * (c.p2 - c.p1);

  double c0_val = (c.p0 - sample.p).dot(sample.tangent);
  double c1_val = p1.dot(sample.tangent);
  double c2_val = p2.dot(sample.tangent);
  double c3_val = p3.dot(sample.tangent);

  auto cubic_roots = solve_cubic(c0_val, c1_val, c2_val, c3_val);
  Roots result;
  for (int i = 0; i < cubic_roots.count; i++)
  {
    double t = cubic_roots.vals[i];
    if (t >= 0.0 && t <= 1.0)
      result.push(t);
  }
  return result;
}

struct CurveDist
{
  std::array<CurveFitSample, N_SAMPLE> samples;
  std::array<double, N_SAMPLE> arcparams{};
  bool arcparams_computed = false;
  double range_start = 0;
  double range_end = 1;
  bool spicy = false;

  static CurveDist from_curve(const SourceCurve& source, double t0, double t1)
  {
    CurveDist cd;
    cd.range_start = t0;
    cd.range_end = t1;
    double step = (t1 - t0) / (N_SAMPLE + 1);
    Vec2 last_tan{};
    bool has_last = false;

    for (int i = 0; i < N_SAMPLE + 2; i++)
    {
      double t = t0 + i * step;
      auto sample = source.sample_pt_tangent(t, 1.0);
      if (has_last)
      {
        double cross_val = std::abs(sample.tangent.cross(last_tan));
        double dot_val = std::abs(sample.tangent.dot(last_tan));
        if (cross_val > SPICY_THRESH * dot_val)
          cd.spicy = true;
      }
      last_tan = sample.tangent;
      has_last = true;
      if (i > 0 && i < N_SAMPLE + 1)
        cd.samples[i - 1] = sample;
    }
    return cd;
  }

  void compute_arc_params(const SourceCurve& source)
  {
    constexpr int N_SUBSAMPLE = 10;
    double dt = (range_end - range_start) / ((N_SAMPLE + 1) * N_SUBSAMPLE);
    double arclen = 0;
    for (int i = 0; i <= N_SAMPLE; i++)
    {
      for (int j = 0; j < N_SUBSAMPLE; j++)
      {
        double t = range_start + dt * (i * N_SUBSAMPLE + j + 0.5);
        auto [p, deriv] = source.sample_pt_deriv(t);
        arclen += deriv.hypot();
      }
      if (i < N_SAMPLE)
        arcparams[i] = arclen;
    }
    if (arclen > 0)
    {
      double inv = 1.0 / arclen;
      for (int i = 0; i < N_SAMPLE; i++)
        arcparams[i] *= inv;
    }
    arcparams_computed = true;
  }

  // Evaluate distance based on arc length parametrization
  std::optional<double> eval_arc(const CubicBez& c, double acc2) const
  {
    constexpr double EPS = 1e-9;
    double c_arclen = c.arclen(EPS);
    double max_err2 = 0;
    for (int i = 0; i < N_SAMPLE; i++)
    {
      double t = c.inv_arclen(c_arclen * arcparams[i], EPS);
      double err = distance_sq(samples[i].p, c.eval(t));
      max_err2 = std::max(err, max_err2);
      if (max_err2 > acc2)
        return std::nullopt;
    }
    return max_err2;
  }

  // Evaluate distance using ray casting
  std::optional<double> eval_ray(const CubicBez& c, double acc2) const
  {
    double max_err2 = 0;
    for (int i = 0; i < N_SAMPLE; i++)
    {
      double best = acc2 + 1.0;
      auto hits = intersect_ray(samples[i], c);
      for (int j = 0; j < hits.count; j++)
      {
        double err = distance_sq(samples[i].p, c.eval(hits.vals[j]));
        best = std::min(best, err);
      }
      max_err2 = std::max(best, max_err2);
      if (max_err2 > acc2)
        return std::nullopt;
    }
    return max_err2;
  }

  std::optional<double> eval_dist(const SourceCurve& source, const CubicBez& c, double acc2)
  {
    auto ray_dist = eval_ray(c, acc2);
    if (!ray_dist)
      return std::nullopt;
    if (!spicy)
      return ray_dist;
    if (!arcparams_computed)
      compute_arc_params(source);
    return eval_arc(c, acc2);
  }
};

// ======================================================================
// Core fitting (ported from fit.rs)
// ======================================================================

// Penalty for long control arms
constexpr double D_PENALTY_ELBOW = 0.65;
constexpr double D_PENALTY_SLOPE = 2.0;

double scale_f(double d)
{
  return 1.0 + std::max(0.0, d - D_PENALTY_ELBOW) * D_PENALTY_SLOPE;
}

// Returns candidate cubics matching area and moment, given unit chord
struct CubicCandidate
{
  CubicBez cubic;
  double d0;
  double d1;
};

std::vector<CubicCandidate> cubic_fit(double th0, double th1, double area, double mx)
{
  double s0 = std::sin(th0);
  double c0 = std::cos(th0);
  double s1 = std::sin(th1);
  double c1 = std::cos(th1);

  double a4 = -9.0 * c0 *
              (((2.0 * s1 * c1 * c0 + s0 * (2.0 * c1 * c1 - 1.0)) * c0 - 2.0 * s1 * c1) * c0 -
               c1 * c1 * s0);
  double a3 =
      12.0 *
      ((((c1 * (30.0 * area * c1 - s1) - 15.0 * area) * c0 +
         2.0 * s0 - c1 * s0 * (c1 + 30.0 * area * s1)) *
            c0 +
        c1 * (s1 - 15.0 * area * c1)) *
           c0 -
       s0 * c1 * c1);
  double a2 =
      12.0 *
      ((((70.0 * mx + 15.0 * area) * s1 * s1 +
         c1 * (9.0 * s1 - 70.0 * c1 * mx - 5.0 * c1 * area)) *
            c0 -
        5.0 * s0 * s1 * (3.0 * s1 - 4.0 * c1 * (7.0 * mx + area))) *
           c0 -
       c1 * (9.0 * s1 - 70.0 * c1 * mx - 5.0 * c1 * area));
  double a1 =
      16.0 *
      (((12.0 * s0 - 5.0 * c0 * (42.0 * mx - 17.0 * area)) * s1 -
        70.0 * c1 * (3.0 * mx - area) * s0 - 75.0 * c0 * c1 * area * area) *
           s1 -
       75.0 * c1 * c1 * area * area * s0);
  double a0 = 80.0 * s1 * (42.0 * s1 * mx - 25.0 * area * (s1 - c1 * area));

  Roots roots;
  constexpr double EPS = 1e-12;

  if (std::abs(a4) > EPS)
  {
    double qa = a3 / a4;
    double qb = a2 / a4;
    double qc = a1 / a4;
    double qd = a0 / a4;

    auto quads = factor_quartic_inner(qa, qb, qc, qd, false);
    if (quads)
    {
      // First quadratic: x^2 + alpha_1*x + beta_1
      auto qr1 = solve_quadratic(quads->b1, quads->a1, 1.0);
      if (qr1.count == 0)
        roots.push(-0.5 * quads->a1);  // real part of complex pair
      else
        for (int i = 0; i < qr1.count; i++)
          roots.push(qr1.vals[i]);

      // Second quadratic: x^2 + alpha_2*x + beta_2
      auto qr2 = solve_quadratic(quads->b2, quads->a2, 1.0);
      if (qr2.count == 0)
        roots.push(-0.5 * quads->a2);
      else
        for (int i = 0; i < qr2.count; i++)
          roots.push(qr2.vals[i]);
    }
  }
  else if (std::abs(a3) > EPS)
  {
    auto cr = solve_cubic(a0, a1, a2, a3);
    for (int i = 0; i < cr.count; i++)
      roots.push(cr.vals[i]);
  }
  else if (std::abs(a2) > EPS || std::abs(a1) > EPS || std::abs(a0) > EPS)
  {
    auto qr = solve_quadratic(a0, a1, a2);
    for (int i = 0; i < qr.count; i++)
      roots.push(qr.vals[i]);
  }
  else
  {
    // Degenerate: return a straight line
    return {{CubicBez({0, 0}, {1.0 / 3.0, 0}, {2.0 / 3.0, 0}, {1, 0}), 1.0 / 3.0, 1.0 / 3.0}};
  }

  double s01 = s0 * c1 + s1 * c0;
  std::vector<CubicCandidate> result;

  for (int i = 0; i < roots.count; i++)
  {
    double d0_val = roots.vals[i];
    double d1_val;
    if (d0_val > 0.0)
    {
      double denom = 0.5 * d0_val * s01 - s1;
      d1_val = (denom != 0.0) ? (d0_val * s0 - area * (10.0 / 3.0)) / denom : 0.0;
      if (d1_val <= 0.0)
      {
        d0_val = (s01 != 0.0) ? s1 / s01 : 0.0;
        d1_val = 0.0;
      }
    }
    else
    {
      d0_val = 0.0;
      d1_val = (s01 != 0.0) ? s0 / s01 : 0.0;
    }

    if (d0_val >= 0.0 && d1_val >= 0.0)
    {
      result.push_back(
          {CubicBez({0, 0}, {d0_val * c0, d0_val * s0}, {1.0 - d1_val * c1, d1_val * s1}, {1, 0}),
           d0_val,
           d1_val});
    }
  }
  return result;
}

// Affine transform: scale, rotate, translate
CubicBez affine_transform(const CubicBez& c, const Point& origin, double th, double scale)
{
  double cos_th = std::cos(th);
  double sin_th = std::sin(th);
  auto xform = [&](const Point& p) -> Point
  {
    double sx = p.x * scale;
    double sy = p.y * scale;
    return {origin.x + sx * cos_th - sy * sin_th, origin.y + sx * sin_th + sy * cos_th};
  };
  return {xform(c.p0), xform(c.p1), xform(c.p2), xform(c.p3)};
}

// Try fitting a line (for very short chords or near-cusps)
std::optional<std::pair<CubicBez, double>> try_fit_line(const SourceCurve& source,
                                                        double accuracy,
                                                        double t0,
                                                        double t1,
                                                        const Point& start,
                                                        const Point& end)
{
  double acc2 = accuracy * accuracy;
  constexpr int SHORT_N = 7;
  double max_err2 = 0;
  double dt = (t1 - t0) / (SHORT_N + 1);
  for (int i = 0; i < SHORT_N; i++)
  {
    double t = t0 + (i + 1) * dt;
    auto [p, d] = source.sample_pt_deriv(t);
    double err2 = line_nearest_dist_sq(start, end, p);
    if (err2 > acc2)
      return std::nullopt;
    max_err2 = std::max(err2, max_err2);
  }
  Point p1 = lerp(start, end, 1.0 / 3.0);
  Point p2 = lerp(end, start, 1.0 / 3.0);
  return std::pair{CubicBez(start, p1, p2, end), max_err2};
}

// Fit a single cubic to a range of the source curve
std::optional<std::pair<CubicBez, double>> fit_to_cubic(const SourceCurve& source,
                                                        double t0,
                                                        double t1,
                                                        double accuracy)
{
  auto start = source.sample_pt_tangent(t0, 1.0);
  auto end = source.sample_pt_tangent(t1, -1.0);
  Vec2 d = end.p - start.p;
  double chord2 = d.hypot2();
  double acc2 = accuracy * accuracy;

  if (chord2 <= acc2)
    return try_fit_line(source, accuracy, t0, t1, start.p, end.p);

  double th = d.atan2();
  auto mod_2pi = [](double angle) -> double
  {
    constexpr double TWO_PI = 2.0 * M_PI;
    double scaled = angle * (0.5 / M_PI);
    return TWO_PI * (scaled - std::round(scaled));
  };
  double th0 = mod_2pi(start.tangent.atan2() - th);
  double th1 = mod_2pi(th - end.tangent.atan2());

  auto [area, mx_raw, my_raw] = source.moment_integrals(t0, t1);
  double x0 = start.p.x;
  double y0 = start.p.y;
  double dx = d.x;
  double dy = d.y;

  // Subtract off area of chord
  area -= dx * (y0 + 0.5 * dy);

  // Subtract off moment of chord
  double dy_3 = dy / 3.0;
  mx_raw -= dx * (x0 * y0 + 0.5 * (x0 * dy + y0 * dx) + dy_3 * dx);
  my_raw -= dx * (y0 * y0 + y0 * dy + dy_3 * dy);

  // Translate start point to origin; convert raw integrals to moments
  mx_raw -= x0 * area;
  my_raw = 0.5 * my_raw - y0 * area;

  // Rotate into place (scaled by chord length)
  double moment = dx * mx_raw + dy * my_raw;

  double chord2_inv = 1.0 / chord2;
  double unit_area = area * chord2_inv;
  double unit_mx = moment * chord2_inv * chord2_inv;

  double chord = std::sqrt(chord2);
  auto curve_dist = CurveDist::from_curve(source, t0, t1);

  std::optional<CubicBez> best_c;
  std::optional<double> best_err2;

  for (auto& [cand, cd0, cd1] : cubic_fit(th0, th1, unit_area, unit_mx))
  {
    auto c = affine_transform(cand, start.p, th, chord);
    auto err2_opt = curve_dist.eval_dist(source, c, acc2);
    if (err2_opt)
    {
      double scale_val = std::max(scale_f(cd0), scale_f(cd1));
      double err2 = *err2_opt * scale_val * scale_val;
      if (err2 < acc2 && (!best_err2 || err2 < *best_err2))
      {
        best_c = c;
        best_err2 = err2;
      }
    }
  }

  if (best_c && best_err2)
    return std::pair{*best_c, *best_err2};
  return std::nullopt;
}

// Recursive Bezier fitting
void fit_to_bezpath_rec(const SourceCurve& source,
                        double t0,
                        double t1,
                        double accuracy,
                        int depth,
                        int maxDepth,
                        BezPath& path)
{
  auto start_p = source.sample_pt_tangent(t0, 1.0).p;
  auto end_p = source.sample_pt_tangent(t1, -1.0).p;

  if (distance_sq(start_p, end_p) <= accuracy * accuracy)
  {
    if (auto result = try_fit_line(source, accuracy, t0, t1, start_p, end_p))
    {
      if (path.empty())
        path.moveTo(result->first.p0);
      path.cubicTo(result->first.p1, result->first.p2, result->first.p3);
      return;
    }
  }

  double t_split;
  auto cusp = source.break_cusp(t0, t1);
  if (cusp)
  {
    t_split = *cusp;
  }
  else if (auto result = fit_to_cubic(source, t0, t1, accuracy))
  {
    if (path.empty())
      path.moveTo(result->first.p0);
    path.cubicTo(result->first.p1, result->first.p2, result->first.p3);
    return;
  }
  else
  {
    t_split = 0.5 * (t0 + t1);
  }

  if (t_split <= t0 || t_split >= t1 || depth >= maxDepth)
  {
    // Fall back to a straight line as a degenerate cubic
    Point p1 = lerp(start_p, end_p, 1.0 / 3.0);
    Point p2 = lerp(end_p, start_p, 1.0 / 3.0);
    if (path.empty())
      path.moveTo(start_p);
    path.cubicTo(p1, p2, end_p);
    return;
  }

  fit_to_bezpath_rec(source, t0, t_split, accuracy, depth + 1, maxDepth, path);
  fit_to_bezpath_rec(source, t_split, t1, accuracy, depth + 1, maxDepth, path);
}

// ======================================================================
// Polyline source curve implementation
// ======================================================================

class PolylineSource : public SourceCurve
{
 public:
  PolylineSource(const std::vector<Point>& pts) : m_points(pts), m_n(pts.size()) {}

  CurveFitSample sample_pt_tangent(double t, double sign) const override
  {
    if (m_n < 2)
      return {m_points[0], {1, 0}};

    double ti = t * (m_n - 1);
    int i = std::clamp(static_cast<int>(std::floor(ti)), 0, static_cast<int>(m_n) - 2);
    double frac = ti - i;

    Point p = lerp(m_points[i], m_points[i + 1], frac);

    // Tangent: at segment junctions, use sign to choose direction
    Vec2 tangent;
    if (frac < 1e-12 && i > 0 && sign < 0)
      tangent = m_points[i] - m_points[i - 1];
    else if (frac > 1.0 - 1e-12 && i < static_cast<int>(m_n) - 2 && sign > 0)
      tangent = m_points[i + 2] - m_points[i + 1];
    else
      tangent = m_points[i + 1] - m_points[i];

    double len = tangent.hypot();
    if (len > 0)
      tangent = tangent * (1.0 / len);
    else
      tangent = {1, 0};

    return {p, tangent};
  }

  std::pair<Point, Vec2> sample_pt_deriv(double t) const override
  {
    if (m_n < 2)
      return {m_points[0], {0, 0}};

    double ti = t * (m_n - 1);
    int i = std::clamp(static_cast<int>(std::floor(ti)), 0, static_cast<int>(m_n) - 2);
    double frac = ti - i;

    Point p = lerp(m_points[i], m_points[i + 1], frac);
    // Derivative = direction * (n-1) (chain rule: dt_internal/dt = n-1)
    Vec2 deriv = (m_points[i + 1] - m_points[i]) * static_cast<double>(m_n - 1);
    return {p, deriv};
  }

 private:
  const std::vector<Point>& m_points;
  std::size_t m_n;
};

// ======================================================================
// SVG output helpers
// ======================================================================

// Fast double-to-string for SVG coordinates (matching OGR-exportToSvg.cpp style)
void append_coord(std::string& out, double num, int decimals)
{
  if (decimals <= 0)
  {
    long rounded = std::lround(num);
    auto s = std::to_string(rounded);
    out.append(s);
    return;
  }

  double rfactor = 1.0;
  for (int i = 0; i < decimals; i++)
    rfactor *= 10.0;
  num = std::round(num * rfactor) / rfactor;

  // Use snprintf for formatting
  char buf[64];
  snprintf(buf, sizeof(buf), "%.*f", decimals, num);

  // Remove trailing zeros after decimal point
  char* end = buf + strlen(buf) - 1;
  while (end > buf && *end == '0')
    --end;
  if (*end == '.' || *end == ',')
    --end;
  out.append(buf, end + 1);
}

}  // anonymous namespace

// ======================================================================
// CubicBez method implementations
// ======================================================================

Point CubicBez::eval(double t) const
{
  double mt = 1.0 - t;
  double mt2 = mt * mt;
  double t2 = t * t;
  double x = mt2 * mt * p0.x + 3.0 * mt2 * t * p1.x + 3.0 * mt * t2 * p2.x + t2 * t * p3.x;
  double y = mt2 * mt * p0.y + 3.0 * mt2 * t * p1.y + 3.0 * mt * t2 * p2.y + t2 * t * p3.y;
  return {x, y};
}

Vec2 CubicBez::deriv_at(double t) const
{
  double mt = 1.0 - t;
  double x = 3.0 * (mt * mt * (p1.x - p0.x) + 2.0 * mt * t * (p2.x - p1.x) + t * t * (p3.x - p2.x));
  double y = 3.0 * (mt * mt * (p1.y - p0.y) + 2.0 * mt * t * (p2.y - p1.y) + t * t * (p3.y - p2.y));
  return {x, y};
}

double CubicBez::arclen(double accuracy) const
{
  // Gauss-Legendre 16-point quadrature of |B'(t)| over [0,1]
  double total = 0;
  for (const auto& [w, xi] : GL16)
  {
    double t = 0.5 * (xi + 1.0);
    auto d = deriv_at(t);
    total += w * d.hypot();
  }
  return total * 0.5;
}

double CubicBez::inv_arclen(double target, double accuracy) const
{
  // ITP-like binary search for t such that arclen(0..t) ≈ target
  double total = arclen(accuracy);
  if (total <= 0)
    return 0;
  if (target >= total)
    return 1;

  double lo = 0;
  double hi = 1;
  for (int i = 0; i < 40; i++)
  {
    double mid = 0.5 * (lo + hi);
    double len = subsegment(0, mid).arclen(accuracy);
    if (len < target)
      lo = mid;
    else
      hi = mid;
  }
  return 0.5 * (lo + hi);
}

CubicBez CubicBez::subsegment(double t0, double t1) const
{
  Point sp0 = eval(t0);
  Point sp3 = eval(t1);
  Vec2 d0 = deriv_at(t0);
  Vec2 d1 = deriv_at(t1);
  double scale = (t1 - t0) / 3.0;
  Point sp1 = sp0 + d0 * scale;
  Point sp2 = sp3 - d1 * scale;
  return {sp0, sp1, sp2, sp3};
}

// ======================================================================
// BezPath methods
// ======================================================================

void BezPath::moveTo(const Point& p)
{
  PathEl el;
  el.type = PathElType::MoveTo;
  el.p1 = p;
  m_elements.push_back(el);
}

void BezPath::cubicTo(const Point& p1, const Point& p2, const Point& p3)
{
  PathEl el;
  el.type = PathElType::CubicTo;
  el.p1 = p1;
  el.p2 = p2;
  el.p3 = p3;
  m_elements.push_back(el);
}

void BezPath::closePath()
{
  PathEl el;
  el.type = PathElType::ClosePath;
  m_elements.push_back(el);
}

std::vector<CubicBez> BezPath::toCubics() const
{
  std::vector<CubicBez> result;
  Point current{0, 0};
  for (const auto& el : m_elements)
  {
    switch (el.type)
    {
      case PathElType::MoveTo:
        current = el.p1;
        break;
      case PathElType::CubicTo:
        result.push_back({current, el.p1, el.p2, el.p3});
        current = el.p3;
        break;
      case PathElType::ClosePath:
        break;
    }
  }
  return result;
}

// ======================================================================
// Public API implementations
// ======================================================================

std::vector<CubicBez> fitPolyline(const std::vector<Point>& points,
                                  double accuracy,
                                  int maxDepth)
{
  if (points.size() < 2)
    return {};

  // Skip degenerate polylines (all points identical)
  bool allSame = true;
  for (std::size_t i = 1; i < points.size(); i++)
  {
    if (points[i].x != points[0].x || points[i].y != points[0].y)
    {
      allSame = false;
      break;
    }
  }
  if (allSame)
    return {};

  PolylineSource source(points);
  BezPath path;
  fit_to_bezpath_rec(source, 0.0, 1.0, accuracy, 0, maxDepth, path);
  return path.toCubics();
}

std::vector<CubicBez> fitPolylineWithBreaks(const std::vector<Point>& points,
                                            const std::vector<int>& breakIndices,
                                            double accuracy,
                                            int maxDepth)
{
  if (points.size() < 2)
    return {};

  // Build complete list of break indices including start and end
  std::vector<int> breaks;
  breaks.push_back(0);
  for (int idx : breakIndices)
  {
    if (idx > 0 && idx < static_cast<int>(points.size()) - 1)
    {
      if (breaks.empty() || breaks.back() != idx)
        breaks.push_back(idx);
    }
  }
  if (breaks.back() != static_cast<int>(points.size()) - 1)
    breaks.push_back(static_cast<int>(points.size()) - 1);

  // Fit each segment between consecutive break points
  std::vector<CubicBez> result;
  for (std::size_t seg = 0; seg + 1 < breaks.size(); seg++)
  {
    int start = breaks[seg];
    int end = breaks[seg + 1];
    if (end - start < 1)
      continue;

    // Extract sub-polyline
    std::vector<Point> subPoints(points.begin() + start, points.begin() + end + 1);
    auto cubics = fitPolyline(subPoints, accuracy, maxDepth);
    result.insert(result.end(), cubics.begin(), cubics.end());
  }
  return result;
}

std::vector<CubicBez> reverseCubics(const std::vector<CubicBez>& cubics)
{
  std::vector<CubicBez> result;
  result.reserve(cubics.size());
  for (auto it = cubics.rbegin(); it != cubics.rend(); ++it)
  {
    result.push_back({it->p3, it->p2, it->p1, it->p0});
  }
  return result;
}

void appendBezierSvg(std::string& out,
                     const std::vector<CubicBez>& cubics,
                     bool emitMoveTo,
                     bool close,
                     int decimals)
{
  if (cubics.empty())
    return;

  if (emitMoveTo)
  {
    out += 'M';
    append_coord(out, cubics.front().p0.x, decimals);
    out += ' ';
    append_coord(out, cubics.front().p0.y, decimals);
  }

  for (const auto& c : cubics)
  {
    out += 'C';
    append_coord(out, c.p1.x, decimals);
    out += ' ';
    append_coord(out, c.p1.y, decimals);
    out += ' ';
    append_coord(out, c.p2.x, decimals);
    out += ' ';
    append_coord(out, c.p2.y, decimals);
    out += ' ';
    append_coord(out, c.p3.x, decimals);
    out += ' ';
    append_coord(out, c.p3.y, decimals);
  }

  if (close)
    out += 'Z';
}

}  // namespace BezierFit
}  // namespace Fmi
