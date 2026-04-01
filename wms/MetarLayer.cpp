// ======================================================================

#ifndef WITHOUT_AVI

#include "MetarLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/avi/Engine.h>
#include <fmt/format.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <spine/Json.h>

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// ======================================================================
// METAR TAC parser implementation
// ======================================================================

namespace
{

// Parse a cloud-layer token: FEW|SCT|BKN|OVC + 3-digit base-hft + optional CB/TCU
bool parseCloudToken(const std::string& tok, MetarData::CloudLayer& layer)
{
  static const std::regex re(R"(^(FEW|SCT|BKN|OVC)(\d{3})(CB|TCU)?$)");
  std::smatch m;
  if (!std::regex_match(tok, m, re))
    return false;

  const std::string& amt = m[1].str();
  if (amt == "FEW")
    layer.amount_oktas = 2;
  else if (amt == "SCT")
    layer.amount_oktas = 4;
  else if (amt == "BKN")
    layer.amount_oktas = 6;
  else
    layer.amount_oktas = 8;  // OVC

  layer.base_hft = std::stoi(m[2].str());

  const std::string& tp = m[3].str();
  if (tp == "TCU")
    layer.type = 1;
  else if (tp == "CB")
    layer.type = 2;
  else
    layer.type = 0;

  return true;
}

// Returns true if the token looks like a METAR present-weather group.
// Handles optional intensity (+/-), optional VC prefix, optional descriptor
// (MI PR BC DR BL SH TS FZ), and phenomena.
bool isWeatherToken(const std::string& tok)
{
  if (tok.empty())
    return false;
  static const std::regex re(
      R"(^[+-]?(?:VC)?(?:MI|PR|BC|DR|BL|SH|TS|FZ)?)"
      R"((?:DZ|RA|SN|SG|IC|PL|GR|GS|UP|BR|FG|FU|VA|DU|SA|HZ|PO|SQ|FC|SS|DS)+$)");
  return std::regex_match(tok, re);
}

}  // namespace

// ======================================================================
// MetarLayer static helpers
// ======================================================================

// static
std::string MetarLayer::formatTemp(int t)
{
  if (t < 0)
    return "M" + (std::abs(t) < 10 ? std::string("0") : std::string{}) +
           Fmi::to_string(std::abs(t));
  return (t < 10 ? std::string("0") : std::string{}) + Fmi::to_string(t);
}

// static
std::string MetarLayer::formatCloudBase(int base_hft)
{
  if (base_hft < 10)
    return "00" + Fmi::to_string(base_hft);
  if (base_hft < 100)
    return "0" + Fmi::to_string(base_hft);
  return Fmi::to_string(base_hft);
}

// static
std::string MetarLayer::cloudAmountCode(int amount_oktas)
{
  if (amount_oktas <= 2)
    return "FEW";
  if (amount_oktas <= 4)
    return "SCT";
  if (amount_oktas <= 7)
    return "BKN";
  return "OVC";
}

// static
std::string MetarLayer::cloudTypeStr(int type)
{
  if (type == 1)
    return "TCU";
  if (type == 2)
    return "CB";
  return {};
}

// static
std::string MetarLayer::weatherString(const MetarData& d)
{
  // Join the raw TAC weather tokens with spaces
  std::string result;
  for (const auto& tok : d.weather_tokens)
  {
    if (!result.empty())
      result += ' ';
    result += tok;
  }
  return result;
}

// static
std::string MetarLayer::skyInfoString(const MetarData& d)
{
  if (d.is_cavok)
    return "CAVOK";
  if (d.is_skc)
    return "SKC";

  // Vertical visibility overrides cloud layers
  if (d.has_vertical_vis)
    return "VV" + formatCloudBase(d.vertical_vis_hft);

  if (d.clouds.empty())
    return {};

  // One cloud layer per line, lowest first (closest to ground = first element)
  std::string result;
  for (const auto& cl : d.clouds)
  {
    if (!result.empty())
      result += '\n';
    result += cloudAmountCode(cl.amount_oktas) + formatCloudBase(cl.base_hft) +
              cloudTypeStr(cl.type);
  }
  return result;
}

// static
std::string MetarLayer::statusColor(const MetarData& d)
{
  // Effective ceiling: vertical vis preferred, else lowest cloud base
  int base_ft = -1;
  if (d.has_vertical_vis)
    base_ft = d.vertical_vis_hft * 100;
  else if (!d.clouds.empty())
    base_ft = d.clouds.front().base_hft * 100;

  int vis_m = -1;
  if (d.is_cavok)
    vis_m = 9999;
  else if (d.has_visibility)
    vis_m = d.visibility;

  if (vis_m < 0 && base_ft < 0)
    return {};  // insufficient data

  auto lt = [](int v, int limit) { return v >= 0 && v < limit; };

  if (lt(vis_m, 800) || lt(base_ft, 200))
    return "#FF0000";  // red   – LIFR
  if (lt(vis_m, 1600) || lt(base_ft, 300))
    return "#FF8C00";  // orange – IFR low
  if (lt(vis_m, 3700) || lt(base_ft, 700))
    return "#FFFF00";  // yellow
  if (lt(vis_m, 5000) || lt(base_ft, 1500))
    return "#00CC00";  // green  – MVFR
  if (lt(vis_m, 8000) || lt(base_ft, 2500))
    return "#FFFFFF";  // white
  return "#0066FF";    // blue   – VFR
}

// ======================================================================
// Wind barb SVG path
// Shaft points FROM the station TOWARD the wind source (upwind direction).
// north_dx/north_dy: unit vector pointing toward geographic north in screen coords.
// In screen coords Y increases downward, so north_dy is typically negative.
// ======================================================================

// static
std::string MetarLayer::windBarbPath(int direction_deg,
                                      int speed_kt,
                                      double north_dx,
                                      double north_dy,
                                      double cx,
                                      double cy,
                                      double shaft_len,
                                      double barb_len)
{
  if (speed_kt < 3)
  {
    // Calm: small open circle centred on station
    double r = barb_len * 0.5;
    return fmt::format(
        "M {:.1f} {:.1f} m -{:.1f} 0 "
        "a {:.1f} {:.1f} 0 1 0 {:.1f} 0 "
        "a {:.1f} {:.1f} 0 1 0 -{:.1f} 0",
        cx, cy, r, r, r, 2 * r, r, r, 2 * r);
  }

  // Direction vector for the shaft (from station toward wind source).
  // In screen space, rotating the north direction vector clockwise by
  // direction_deg gives the wind-from direction.  Because screen Y is
  // flipped relative to math Y, a clockwise geographic rotation maps to
  // the CCW formula: x' = x·cosθ - y·sinθ, y' = x·sinθ + y·cosθ.
  const double theta = direction_deg * M_PI / 180.0;
  const double sdx = north_dx * std::cos(theta) - north_dy * std::sin(theta);
  const double sdy = north_dx * std::sin(theta) + north_dy * std::cos(theta);

  // Shaft tip (barbs are drawn here)
  const double tx = cx + sdx * shaft_len;
  const double ty = cy + sdy * shaft_len;

  // Right-perpendicular to shaft when looking from station toward tip
  const double pdx = -sdy;
  const double pdy = sdx;

  std::ostringstream path;
  path.precision(1);
  path << std::fixed;

  // Main shaft
  path << "M " << cx << " " << cy << " L " << tx << " " << ty;

  int kt = speed_kt;
  int pennants  = kt / 50;  kt -= pennants * 50;
  int full_barbs = kt / 10;  kt -= full_barbs * 10;
  int half_barbs = kt / 5;

  int n_barbs = pennants + full_barbs + half_barbs;
  double spacing = (n_barbs > 0) ? std::min(shaft_len / (n_barbs + 1), barb_len * 0.9) : 0.0;

  double bx = tx;
  double by = ty;

  for (int i = 0; i < pennants; i++)
  {
    double b2x = bx - sdx * spacing;
    double b2y = by - sdy * spacing;
    path << " M " << bx << " " << by
         << " L " << (bx + pdx * barb_len) << " " << (by + pdy * barb_len)
         << " L " << b2x << " " << b2y << " Z";
    bx = b2x;
    by = b2y;
  }

  for (int i = 0; i < full_barbs; i++)
  {
    path << " M " << bx << " " << by
         << " L " << (bx + pdx * barb_len) << " " << (by + pdy * barb_len);
    bx -= sdx * spacing;
    by -= sdy * spacing;
  }

  for (int i = 0; i < half_barbs; i++)
  {
    path << " M " << bx << " " << by
         << " L " << (bx + pdx * barb_len * 0.5) << " " << (by + pdy * barb_len * 0.5);
    bx -= sdx * spacing;
    by -= sdy * spacing;
  }

  return path.str();
}

// ======================================================================
// METAR TAC parser
// ======================================================================

// static
MetarData MetarLayer::parseTAC(const std::string& message,
                                double lon,
                                double lat,
                                const std::string& icao_hint)
{
  MetarData d;
  d.longitude = lon;
  d.latitude = lat;
  d.icao = icao_hint;

  // Tokenise on whitespace
  std::istringstream iss(message);
  std::vector<std::string> tokens;
  std::string tok;
  while (iss >> tok)
    tokens.push_back(tok);

  if (tokens.empty())
    return d;

  std::size_t idx = 0;

  // Skip type prefix: METAR / SPECI / AUTO / COR
  if (idx < tokens.size() &&
      (tokens[idx] == "METAR" || tokens[idx] == "SPECI"))
    ++idx;
  while (idx < tokens.size() &&
         (tokens[idx] == "AUTO" || tokens[idx] == "COR" || tokens[idx] == "NIL"))
    ++idx;

  // Station ICAO
  if (idx < tokens.size() && tokens[idx].size() == 4 && std::isalpha(tokens[idx][0]))
  {
    d.icao = tokens[idx];
    ++idx;
  }

  // Time group: DDHHMMz
  if (idx < tokens.size())
  {
    static const std::regex time_re(R"(^\d{6}Z$)");
    if (std::regex_match(tokens[idx], time_re))
      ++idx;
  }

  // Possibly more AUTO/COR/NIL
  while (idx < tokens.size() &&
         (tokens[idx] == "AUTO" || tokens[idx] == "COR" || tokens[idx] == "NIL"))
    ++idx;

  // Compiled regexes for token identification
  static const std::regex wind_re(R"(^(VRB|\d{3})(\d{2,3})(G(\d{2,3}))?KT$)");
  static const std::regex vis4_re(R"(^(\d{4})$)");
  static const std::regex rvr_re(R"(^R\d{2}[LCR]?/.+$)");
  static const std::regex cloud_re(R"(^(FEW|SCT|BKN|OVC)(\d{3})(CB|TCU)?$)");
  static const std::regex vv_re(R"(^VV(\d{3}|///)$)");
  static const std::regex tempdew_re(R"(^(M?\d{2})/(M?\d{2})$)");
  static const std::regex qnh_re(R"(^Q(\d{4})$)");
  static const std::regex alt_re(R"(^A(\d{4})$)");
  static const std::regex varwind_re(R"(^\d{3}V\d{3}$)");

  // State machine
  enum class S { Wind, Vis, RVR, WX, Clouds, TempDew, Done };
  S state = S::Wind;

  std::smatch m;

  for (; idx < tokens.size(); ++idx)
  {
    const std::string& t = tokens[idx];

    // Hard terminators
    if (t == "NOSIG" || t == "BECMG" || t == "TEMPO" || t == "RMK" || t == "=" ||
        t == "INTER")
      break;

    switch (state)
    {
      // ----------------------------------------------------------
      case S::Wind:
        if (t == "CAVOK")
        {
          d.is_cavok = true;
          d.has_visibility = true;
          d.visibility = 9999;
          state = S::TempDew;
          break;
        }
        if (std::regex_match(t, m, wind_re))
        {
          d.has_wind = true;
          d.wind_variable = (m[1].str() == "VRB");
          if (!d.wind_variable)
            d.wind_direction = std::stoi(m[1].str());
          d.wind_speed = std::stoi(m[2].str());
          if (m[4].matched)
          {
            d.has_gust = true;
            d.wind_gust = std::stoi(m[4].str());
          }
          state = S::Vis;
          break;
        }
        if (std::regex_match(t, varwind_re))
          break;  // wind direction variability group, skip
        // Token doesn't look like wind – fall through to Vis
        state = S::Vis;
        [[fallthrough]];

      // ----------------------------------------------------------
      case S::Vis:
        if (t == "CAVOK")
        {
          d.is_cavok = true;
          d.has_visibility = true;
          d.visibility = 9999;
          state = S::TempDew;
          break;
        }
        if (t == "9999")
        {
          d.has_visibility = true;
          d.visibility = 9999;
          state = S::RVR;
          break;
        }
        if (std::regex_match(t, m, vis4_re))
        {
          d.has_visibility = true;
          d.visibility = std::min(std::stoi(m[1].str()), 9999);
          state = S::RVR;
          break;
        }
        // Unrecognised visibility token; advance state and fall through
        state = S::RVR;
        [[fallthrough]];

      // ----------------------------------------------------------
      case S::RVR:
        if (std::regex_match(t, rvr_re))
          break;  // skip RVR groups
        state = S::WX;
        [[fallthrough]];

      // ----------------------------------------------------------
      case S::WX:
        if (t == "SKC" || t == "CLR" || t == "NSC" || t == "NCD")
        {
          d.is_skc = true;
          state = S::Clouds;
          break;
        }
        if (std::regex_match(t, m, vv_re))
        {
          const std::string& vv = m[1].str();
          if (vv != "///")
          {
            d.has_vertical_vis = true;
            d.vertical_vis_hft = std::stoi(vv);
          }
          state = S::TempDew;
          break;
        }
        if (std::regex_match(t, cloud_re))
        {
          state = S::Clouds;
          MetarData::CloudLayer cl;
          parseCloudToken(t, cl);
          d.clouds.push_back(cl);
          break;
        }
        if (std::regex_match(t, tempdew_re))
        {
          state = S::TempDew;
          goto handle_tempdew;
        }
        if (isWeatherToken(t) && d.weather_tokens.size() < 3)
        {
          d.weather_tokens.push_back(t);
          break;
        }
        // Might be a cloud or temp group appearing without expected WX
        state = S::Clouds;
        [[fallthrough]];

      // ----------------------------------------------------------
      case S::Clouds:
        if (t == "SKC" || t == "CLR" || t == "NSC" || t == "NCD")
        {
          d.is_skc = true;
          break;
        }
        if (std::regex_match(t, m, vv_re))
        {
          const std::string& vv = m[1].str();
          if (vv != "///")
          {
            d.has_vertical_vis = true;
            d.vertical_vis_hft = std::stoi(vv);
          }
          state = S::TempDew;
          break;
        }
        if (std::regex_match(t, cloud_re))
        {
          MetarData::CloudLayer cl;
          parseCloudToken(t, cl);
          d.clouds.push_back(cl);
          break;
        }
        // Check for temp/dew appearing right after clouds
        if (std::regex_match(t, tempdew_re))
        {
          state = S::TempDew;
          goto handle_tempdew;
        }
        // Try QNH too
        if (std::regex_match(t, m, qnh_re))
        {
          d.has_pressure = true;
          d.pressure = std::stoi(m[1].str());
          break;
        }
        if (std::regex_match(t, m, alt_re))
        {
          double inHg = std::stod(m[1].str()) / 100.0;
          d.has_pressure = true;
          d.pressure = static_cast<int>(std::round(inHg * 33.8639));
          break;
        }
        state = S::TempDew;
        [[fallthrough]];

      // ----------------------------------------------------------
      case S::TempDew:
      handle_tempdew:
        if (std::regex_match(t, m, tempdew_re))
        {
          auto parse_td = [](const std::string& s) -> int {
            bool neg = (!s.empty() && s[0] == 'M');
            int v = std::stoi(s.substr(neg ? 1 : 0));
            return neg ? -v : v;
          };
          d.has_temperature = true;
          d.temperature = parse_td(m[1].str());
          d.has_dewpoint = true;
          d.dewpoint = parse_td(m[2].str());
          state = S::Done;
          break;
        }
        if (std::regex_match(t, m, qnh_re))
        {
          d.has_pressure = true;
          d.pressure = std::stoi(m[1].str());
          break;
        }
        if (std::regex_match(t, m, alt_re))
        {
          double inHg = std::stod(m[1].str()) / 100.0;
          d.has_pressure = true;
          d.pressure = static_cast<int>(std::round(inHg * 33.8639));
          break;
        }
        break;

      // ----------------------------------------------------------
      case S::Done:
        // Check for QNH that appears after temp/dew
        if (std::regex_match(t, m, qnh_re))
        {
          d.has_pressure = true;
          d.pressure = std::stoi(m[1].str());
        }
        else if (std::regex_match(t, m, alt_re))
        {
          double inHg = std::stod(m[1].str()) / 100.0;
          d.has_pressure = true;
          d.pressure = static_cast<int>(std::round(inHg * 33.8639));
        }
        break;
    }
  }

  // Final sweep for QNH in case it appeared in an unexpected position
  if (!d.has_pressure)
  {
    for (const auto& token : tokens)
    {
      if (std::regex_match(token, m, qnh_re))
      {
        d.has_pressure = true;
        d.pressure = std::stoi(m[1].str());
        break;
      }
      if (std::regex_match(token, m, alt_re))
      {
        double inHg = std::stod(m[1].str()) / 100.0;
        d.has_pressure = true;
        d.pressure = static_cast<int>(std::round(inHg * 33.8639));
        break;
      }
    }
  }

  return d;
}

// ======================================================================
// init
// ======================================================================

void MetarLayer::init(Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    Layer::init(theJson, theState, theConfig, theProperties);

    JsonTools::remove_string(message_type, theJson, "message_type");
    JsonTools::remove_string(message_format, theJson, "message_format");
    JsonTools::remove_bool(exclude_specis, theJson, "exclude_specis");
    JsonTools::remove_int(font_size, theJson, "font_size");
    JsonTools::remove_int(plot_size, theJson, "plot_size");
    JsonTools::remove_bool(show_temperature, theJson, "show_temperature");
    JsonTools::remove_bool(show_dewpoint, theJson, "show_dewpoint");
    JsonTools::remove_bool(show_visibility, theJson, "show_visibility");
    JsonTools::remove_bool(show_status, theJson, "show_status");
    JsonTools::remove_bool(show_wind, theJson, "show_wind");
    JsonTools::remove_bool(show_pressure, theJson, "show_pressure");
    JsonTools::remove_bool(show_gust, theJson, "show_gust");
    JsonTools::remove_bool(show_weather, theJson, "show_weather");
    JsonTools::remove_bool(show_sky_info, theJson, "show_sky_info");
    JsonTools::remove_string(color, theJson, "color");
    JsonTools::remove_int(mindistance, theJson, "mindistance");

    auto icao_json = JsonTools::remove(theJson, "icaos");
    if (!icao_json.isNull() && icao_json.isArray())
      for (const auto& j : icao_json)
        icaos.push_back(j.asString());

    auto country_json = JsonTools::remove(theJson, "countries");
    if (!country_json.isNull() && country_json.isArray())
      for (const auto& j : country_json)
        countries.push_back(j.asString());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "MetarLayer::init failed!");
  }
}

// ======================================================================
// hash_value
// ======================================================================

std::size_t MetarLayer::hash_value(const State& theState) const
{
  try
  {
    std::size_t seed = Layer::hash_value(theState);
    Fmi::hash_combine(seed, Fmi::hash_value(message_type));
    Fmi::hash_combine(seed, Fmi::hash_value(message_format));
    Fmi::hash_combine(seed, Fmi::hash_value(exclude_specis));
    Fmi::hash_combine(seed, Fmi::hash_value(font_size));
    Fmi::hash_combine(seed, Fmi::hash_value(plot_size));
    Fmi::hash_combine(seed, Fmi::hash_value(show_temperature));
    Fmi::hash_combine(seed, Fmi::hash_value(show_dewpoint));
    Fmi::hash_combine(seed, Fmi::hash_value(show_visibility));
    Fmi::hash_combine(seed, Fmi::hash_value(show_status));
    Fmi::hash_combine(seed, Fmi::hash_value(show_wind));
    Fmi::hash_combine(seed, Fmi::hash_value(show_pressure));
    Fmi::hash_combine(seed, Fmi::hash_value(show_gust));
    Fmi::hash_combine(seed, Fmi::hash_value(show_weather));
    Fmi::hash_combine(seed, Fmi::hash_value(show_sky_info));
    Fmi::hash_combine(seed, Fmi::hash_value(color));
    Fmi::hash_combine(seed, Fmi::hash_value(mindistance));
    for (const auto& s : icaos)
      Fmi::hash_combine(seed, Fmi::hash_value(s));
    for (const auto& s : countries)
      Fmi::hash_combine(seed, Fmi::hash_value(s));
    return seed;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "MetarLayer::hash_value failed!");
  }
}

// ======================================================================
// getFeatureInfo (stub)
// ======================================================================

void MetarLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
}

// ======================================================================
// generate
// ======================================================================

void MetarLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "MetarLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    if (!validLayer(theState))
      return;

    // ----------------------------------------------------------------
    // Projection and coordinate transformation
    // ----------------------------------------------------------------
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();
    Fmi::CoordinateTransformation trans("WGS84", crs);

    // Map corners in WGS84 for the AVI engine bbox query
    const auto& sw = projection.bottomLeftLatLon();
    const auto& ne = projection.topRightLatLon();

    // ----------------------------------------------------------------
    // Build AVI engine query
    // ----------------------------------------------------------------
    Engine::Avi::QueryOptions opts;

    opts.itsParameters.emplace_back("icao");
    opts.itsParameters.emplace_back("messagetime");
    opts.itsParameters.emplace_back("message");
    opts.itsParameters.emplace_back("longitude");
    opts.itsParameters.emplace_back("latitude");

    opts.itsValidity = Engine::Avi::Validity::Accepted;
    opts.itsMessageTypes.push_back(message_type);
    opts.itsMessageFormat = message_format;

    // Time: format as PostgreSQL timestamptz literal accepted by the AVI engine
    auto valid_time = getValidTime();
    std::string ts = Fmi::to_iso_string(valid_time);  // "YYYYMMDDTHHmmSS"
    opts.itsTimeOptions.itsObservationTime = "timestamptz '" + ts + "Z'";
    opts.itsTimeOptions.itsTimeFormat = "iso";
    opts.itsTimeOptions.itsMessageTimeChecks = false;
    opts.itsTimeOptions.itsQueryValidRangeMessages = true;

    opts.itsDistinctMessages = true;
    opts.itsFilterMETARs = (message_format == "TAC");
    opts.itsExcludeSPECIs = exclude_specis;
    opts.itsMaxMessageStations = -1;
    opts.itsMaxMessageRows = -1;

    // Location filtering: explicit ICAOs or countries override map bbox
    if (!icaos.empty())
    {
      for (const auto& ic : icaos)
        opts.itsLocationOptions.itsIcaos.push_back(ic);
    }
    else if (!countries.empty())
    {
      for (const auto& c : countries)
        opts.itsLocationOptions.itsIncludeCountryFilters.push_back(c);
    }
    else
    {
      // Use map bounding box, expanded slightly to capture border stations
      Engine::Avi::BBox avibox(sw.X() - 0.5, ne.X() + 0.5, sw.Y() - 0.5, ne.Y() + 0.5);
      opts.itsLocationOptions.itsBBoxes.push_back(avibox);
    }

    // ----------------------------------------------------------------
    // Execute query
    // ----------------------------------------------------------------
    auto& aviEngine = theState.getAviEngine();
    auto aviData = aviEngine.queryStationsAndMessages(opts);

    // ----------------------------------------------------------------
    // Parse messages and transform to pixel coordinates
    // ----------------------------------------------------------------
    struct StationPlot
    {
      int x = 0;
      int y = 0;
      MetarData data;
    };

    std::vector<StationPlot> plots;
    plots.reserve(aviData.itsStationIds.size());

    for (auto stationId : aviData.itsStationIds)
    {
      auto& vals = aviData.itsValues[stationId];

      if (vals.find("longitude") == vals.end() ||
          vals.find("latitude") == vals.end() ||
          vals.find("message") == vals.end())
        continue;

      if (vals.at("longitude").empty() || vals.at("latitude").empty() ||
          vals.at("message").empty())
        continue;

      double lon = std::get<double>(*vals.at("longitude").cbegin());
      double lat = std::get<double>(*vals.at("latitude").cbegin());

      const auto& msg_val = *vals.at("message").cbegin();
      if (!std::holds_alternative<std::string>(msg_val))
        continue;
      const std::string& raw_message = std::get<std::string>(msg_val);

      std::string icao_str;
      if (vals.count("icao") && !vals.at("icao").empty())
      {
        const auto& iv = *vals.at("icao").cbegin();
        if (std::holds_alternative<std::string>(iv))
          icao_str = std::get<std::string>(iv);
      }

      // Transform to pixel
      double px = lon;
      double py = lat;
      if (!crs.isGeographic() && !trans.transform(px, py))
        continue;
      box.transform(px, py);

      if (!inside(box, px, py))
        continue;

      MetarData d = parseTAC(raw_message, lon, lat, icao_str);
      plots.push_back({static_cast<int>(std::lround(px)),
                       static_cast<int>(std::lround(py)),
                       std::move(d)});
    }

    // ----------------------------------------------------------------
    // Declutter: drop stations whose bounding box overlaps an already-
    // rendered one.  Sort by ICAO so well-known airports (shorter/earlier
    // codes) tend to be drawn first.
    // ----------------------------------------------------------------
    std::sort(plots.begin(), plots.end(), [](const StationPlot& a, const StationPlot& b) {
      return a.data.icao < b.data.icao;
    });

    const int B = (plot_size > 0) ? plot_size : 6 * font_size;
    const int min_dist = (mindistance > 0) ? mindistance : B;

    std::vector<StationPlot> rendered;
    rendered.reserve(plots.size());
    for (auto& p : plots)
    {
      bool too_close = false;
      for (const auto& r : rendered)
      {
        if (std::abs(p.x - r.x) < min_dist && std::abs(p.y - r.y) < min_dist)
        {
          too_close = true;
          break;
        }
      }
      if (!too_close)
        rendered.push_back(p);
    }

    // ----------------------------------------------------------------
    // CSS
    // ----------------------------------------------------------------
    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // ----------------------------------------------------------------
    // SVG generation
    // ----------------------------------------------------------------
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Outer SVG group
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";
    theState.addAttributes(theGlobals, group_cdt, attributes);
    theLayersCdt.PushBack(group_cdt);

    const std::string txt_color = color.empty() ? "black" : color;
    const double shaft_len = B * 0.32;
    const double barb_len  = B * 0.14;
    const int    box_sz    = std::max(4, font_size - 2);
    const int    line_h    = static_cast<int>(font_size * 1.25);

    for (const auto& p : rendered)
    {
      const int sx = p.x;
      const int sy = p.y;
      const auto& d = p.data;

      // Compute geographic-north direction in screen coordinates for
      // true-north correction of the wind barb.
      double north_dx = 0.0, north_dy = -1.0;
      {
        double x1 = d.longitude, y1 = d.latitude;
        double x2 = d.longitude, y2 = d.latitude + 0.1;
        bool ok = true;
        if (!crs.isGeographic())
          ok = trans.transform(x1, y1) && trans.transform(x2, y2);
        if (ok)
        {
          box.transform(x1, y1);
          box.transform(x2, y2);
          double dx = x2 - x1, dy = y2 - y1;
          double len = std::sqrt(dx * dx + dy * dy);
          if (len > 1e-6)
          {
            north_dx = dx / len;
            north_dy = dy / len;
          }
        }
      }

      // Plot-layout offsets (from PDF, relative coords 0-1, station at 0.5,0.5):
      //   T:       (0.35, 0.9)  → dx = -0.15*B, dy = -0.40*B  right-aligned
      //   Vis:     (0.35, 0.675)→ dx = -0.15*B, dy = -0.175*B right-aligned
      //   Td:      (0.35, 0.45) → dx = -0.15*B, dy = +0.05*B  right-aligned
      //   Status+Wind at centre (0, 0)
      //   P:       (0.6,  0.9)  → dx = +0.10*B, dy = -0.40*B  left-aligned
      //   Gust:    (0.6,  0.675)→ dx = +0.10*B, dy = -0.175*B left-aligned
      //   Weather: (0.6,  0.45) → dx = +0.10*B, dy = +0.05*B  left-aligned
      //   Sky:     (0.5,  0.25) → dx = 0,        dy = +0.25*B  centre-aligned

      auto ox = [&](double rel) { return sx + static_cast<int>(std::lround(rel * B)); };
      auto oy = [&](double rel) { return sy + static_cast<int>(std::lround(rel * B)); };

      // -- Status colour box (centred on station) --
      if (show_status)
      {
        std::string sc = statusColor(d);
        if (!sc.empty())
        {
          CTPP::CDT r(CTPP::CDT::HASH_VAL);
          r["start"] = "<rect";
          r["end"] = "/>";
          r["attributes"]["x"] = Fmi::to_string(sx - box_sz / 2);
          r["attributes"]["y"] = Fmi::to_string(sy - box_sz / 2);
          r["attributes"]["width"] = Fmi::to_string(box_sz);
          r["attributes"]["height"] = Fmi::to_string(box_sz);
          r["attributes"]["fill"] = (color.empty() ? sc : color);
          r["attributes"]["stroke"] = "black";
          r["attributes"]["stroke-width"] = "0.5";
          theLayersCdt.PushBack(r);
        }
      }

      // -- Wind barb --
      if (show_wind && d.has_wind)
      {
        int spd = d.wind_variable ? 0 : d.wind_speed;
        int dir = d.wind_variable ? 0 : d.wind_direction;
        std::string bp =
            windBarbPath(dir, spd, north_dx, north_dy,
                         static_cast<double>(sx), static_cast<double>(sy),
                         shaft_len, barb_len);
        if (!bp.empty())
        {
          CTPP::CDT pc(CTPP::CDT::HASH_VAL);
          pc["start"] = "<path";
          pc["end"] = "/>";
          pc["attributes"]["d"] = bp;
          pc["attributes"]["stroke"] = txt_color;
          pc["attributes"]["stroke-width"] = "1.5";
          pc["attributes"]["fill"] = txt_color;
          pc["attributes"]["stroke-linecap"] = "round";
          theLayersCdt.PushBack(pc);
        }
      }

      // Helper: push a single <text> CDT
      auto push_text = [&](const std::string& text_val,
                           int x,
                           int y,
                           const std::string& anchor)
      {
        CTPP::CDT tc(CTPP::CDT::HASH_VAL);
        tc["start"] = "<text";
        tc["end"] = "</text>";
        tc["cdata"] = text_val;
        tc["attributes"]["x"] = Fmi::to_string(x);
        tc["attributes"]["y"] = Fmi::to_string(y);
        tc["attributes"]["text-anchor"] = anchor;
        tc["attributes"]["font-size"] = Fmi::to_string(font_size);
        tc["attributes"]["fill"] = txt_color;
        theLayersCdt.PushBack(tc);
      };

      // -- Temperature (upper left, right-aligned) --
      if (show_temperature && d.has_temperature)
        push_text(formatTemp(d.temperature), ox(-0.15), oy(-0.40), "end");

      // -- Visibility (middle left, right-aligned) --
      if (show_visibility && d.has_visibility && !d.is_cavok && !d.is_skc)
        push_text(Fmi::to_string(d.visibility), ox(-0.15), oy(-0.175), "end");

      // -- Dew point (lower left, right-aligned) --
      if (show_dewpoint && d.has_dewpoint)
        push_text(formatTemp(d.dewpoint), ox(-0.15), oy(+0.05), "end");

      // -- QNH pressure (upper right, left-aligned) --
      if (show_pressure && d.has_pressure)
        push_text(Fmi::to_string(d.pressure), ox(+0.10), oy(-0.40), "start");

      // -- Wind gust (middle right, left-aligned) --
      if (show_gust && d.has_gust)
        push_text("G" + Fmi::to_string(d.wind_gust), ox(+0.10), oy(-0.175), "start");

      // -- Present weather (lower right, left-aligned) --
      if (show_weather && !d.weather_tokens.empty())
      {
        const std::string wx = weatherString(d);
        if (!wx.empty())
          push_text(wx, ox(+0.10), oy(+0.05), "start");
      }

      // -- Sky info (below centre, centre-aligned, one line per layer) --
      if (show_sky_info)
      {
        const std::string sky = skyInfoString(d);
        if (!sky.empty())
        {
          int tx = sx;
          int ty = oy(+0.25);
          std::istringstream sky_ss(sky);
          std::string line;
          int n = 0;
          while (std::getline(sky_ss, line))
          {
            if (!line.empty())
              push_text(line, tx, ty + n * line_h, "middle");
            ++n;
          }
        }
      }
    }  // for each rendered station

    // Close the outer <g> by appending to the last CDT's "end" field
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n</g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "MetarLayer::generate failed!")
        .addParameter("qid", qid)
        .addParameter("message_type", message_type);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_AVI
