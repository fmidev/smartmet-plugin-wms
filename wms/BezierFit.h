// ======================================================================
/*!
 * \brief Bezier curve fitting using moment-matching (port of Kurbo's fit.rs)
 *
 * This implements Raph Levien's moment-matching algorithm for fitting
 * cubic Bezier curves to polylines. The algorithm works by computing
 * signed area and first moment of the source curve segment, then
 * solving a quartic polynomial to find cubic Bezier control points
 * that match those moments.
 *
 * Reference: https://github.com/linebender/kurbo
 * Original license: Apache-2.0 OR MIT
 */
// ======================================================================

#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace Fmi
{
namespace BezierFit
{
// ---- Basic types ----

struct Point
{
  double x = 0;
  double y = 0;
  Point() = default;
  Point(double x_, double y_) : x(x_), y(y_) {}
};

struct Vec2
{
  double x = 0;
  double y = 0;
  Vec2() = default;
  Vec2(double x_, double y_) : x(x_), y(y_) {}

  double dot(const Vec2& o) const { return x * o.x + y * o.y; }
  double cross(const Vec2& o) const { return x * o.y - y * o.x; }
  double hypot2() const { return x * x + y * y; }
  double hypot() const { return std::hypot(x, y); }
  double atan2() const { return std::atan2(y, x); }
};

inline Point operator+(const Point& p, const Vec2& v) { return {p.x + v.x, p.y + v.y}; }
inline Point operator-(const Point& p, const Vec2& v) { return {p.x - v.x, p.y - v.y}; }
inline Vec2 operator-(const Point& a, const Point& b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(const Vec2& v, double s) { return {v.x * s, v.y * s}; }
inline Vec2 operator*(double s, const Vec2& v) { return {v.x * s, v.y * s}; }
inline Vec2 operator+(const Vec2& a, const Vec2& b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(const Vec2& a, const Vec2& b) { return {a.x - b.x, a.y - b.y}; }

inline Point lerp(const Point& a, const Point& b, double t) {
  return {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
}

inline double distance_sq(const Point& a, const Point& b) {
  double dx = a.x - b.x, dy = a.y - b.y;
  return dx * dx + dy * dy;
}

// ---- Cubic Bezier ----

struct CubicBez
{
  Point p0, p1, p2, p3;

  CubicBez() = default;
  CubicBez(const Point& p0_, const Point& p1_, const Point& p2_, const Point& p3_)
      : p0(p0_), p1(p1_), p2(p2_), p3(p3_)
  {
  }

  Point eval(double t) const;
  Vec2 deriv_at(double t) const;
  double arclen(double accuracy) const;
  double inv_arclen(double target, double accuracy) const;
  CubicBez subsegment(double t0, double t1) const;
};

// ---- Bezier path (sequence of path elements) ----

enum class PathElType
{
  MoveTo,
  CubicTo,
  ClosePath
};

struct PathEl
{
  PathElType type;
  Point p1, p2, p3;  // MoveTo uses p1; CubicTo uses all three
};

class BezPath
{
 public:
  void moveTo(const Point& p);
  void cubicTo(const Point& p1, const Point& p2, const Point& p3);
  void closePath();

  bool empty() const { return m_elements.empty(); }
  const std::vector<PathEl>& elements() const { return m_elements; }

  // Extract the cubic segments
  std::vector<CubicBez> toCubics() const;

 private:
  std::vector<PathEl> m_elements;
};

// ---- Fitting API ----

// Fit a polyline sub-segment to cubic Bezier curves.
// points: the polyline vertices (at least 2)
// accuracy: maximum allowed approximation error (in the same units as the coordinates)
// maxDepth: maximum recursion depth for subdivision (default 10)
std::vector<CubicBez> fitPolyline(const std::vector<Point>& points,
                                  double accuracy,
                                  int maxDepth = 10);

// Fit an entire polyline with explicit break indices where corners must be preserved.
// breakIndices: sorted indices of vertices where the curve must have a corner.
// The first and last vertex are always implicit break points.
std::vector<CubicBez> fitPolylineWithBreaks(const std::vector<Point>& points,
                                            const std::vector<int>& breakIndices,
                                            double accuracy,
                                            int maxDepth = 10);

// Reverse a sequence of cubics (same geometric path, opposite direction)
std::vector<CubicBez> reverseCubics(const std::vector<CubicBez>& cubics);

// ---- SVG output ----

// Append SVG path data for a sequence of cubics.
// If emitMoveTo is true, starts with "M x y"
// If close is true, ends with "Z"
void appendBezierSvg(std::string& out,
                     const std::vector<CubicBez>& cubics,
                     bool emitMoveTo,
                     bool close,
                     int decimals);

}  // namespace BezierFit
}  // namespace Fmi
