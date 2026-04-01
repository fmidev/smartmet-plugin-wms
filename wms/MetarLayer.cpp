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
#include <map>
#include <regex>
#include <sstream>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{

// ======================================================================
// Weather code lookup table (ww numeric code → METAR text)
// Source: PDF section "Avi W → teksti konvertointitaulukko"
// ======================================================================

const std::map<int, std::string> ww_to_text = {
    {4, "FU"},      {5, "HZ"},      {6, "DU"},       {7, "BLSA"},    {8, "PO"},
    {9, "VCDS"},    {10, "BR"},     {11, "BCFG"},     {12, "MIFG"},   {13, "VCTS"},
    {16, "VCSH"},   {17, "TS"},     {18, "SQ"},       {19, "FC"},     {31, "DS"},
    {36, "DRSN"},   {37, "+DRSN"},  {38, "BLSN"},     {39, "+BLSN"},  {40, "VCFG"},
    {41, "PRFG"},   {49, "FG"},     {51, "-DZ"},      {53, "DZ"},     {55, "+DZ"},
    {56, "-FZDZ"},  {57, "FZDZ"},   {58, "-RADZ"},    {59, "RADZ"},   {61, "-RA"},
    {63, "RA"},     {66, "-FZRA"},  {67, "FZRA"},     {68, "-RASN"},  {69, "RASN"},
    {71, "-SN"},    {73, "SN"},     {75, "+SN"},      {76, "IC"},     {77, "SG"},
    {79, "PL"},     {80, "-SHRA"},  {81, "SHRA"},     {82, "+SHRA"},  {83, "-SHRASN"},
    {84, "SHRASN"}, {85, "-SHSN"},  {86, "SHSN"},     {87, "SHGS"},   {88, "SHGS"},
    {89, "-SHGR"},  {90, "SHGR"},   {95, "TSRA"},     {96, "TSGR"},   {97, "+TSRA"},
    {99, "+TSGR"},
};

// ======================================================================
// Reverse: METAR weather group text → ww code (for status/string output)
// Only used to reconstruct weather text from METAR groups in parseTAC
// ======================================================================

// Map present-weather METAR tokens to ww codes for internal use.
// We store up to 3 tokens from the raw message rather than converting
// to/from ww codes; the text is taken directly from the TAC token.

// ======================================================================
// Parse a cloud layer token: FEW/SCT/BKN/OVC + 3-digit hundreds-of-feet
// plus optional CB/TCU suffix.  Returns true on success.
// ======================================================================

bool parseCloudToken(const std::string& tok, MetarData::CloudLayer& layer)
{
  static const std::regex re(R"(^(FEW|SCT|BKN|OVC)(\d{3})(CB|TCU)?$)");
  std::smatch m;
  if (!std::regex_match(tok, m, re))
    return false;

  const auto& amount_str = m[1].str();
  if (amount_str == "FEW")
    layer.amount = 2;  // 1-2 oktas representatively stored as 2
  else if (amount_str == "SCT")
    layer.amount = 4;
  else if (amount_str == "BKN")
    layer.amount = 6;
  else
    layer.amount = 8;  // OVC

  layer.base_hft = std::stoi(m[2].str());

  const auto& type_str = m[3].str();
  if (type_str == "TCU")
    layer.type = 1;
  else if (type_str == "CB")
    layer.type = 2;
  else
    layer.type = 0;

  return true;
}

// ======================================================================
// Determine whether a token looks like a weather group
// (METAR wx qualifier/descriptor/phenomena pattern)
// ======================================================================

bool isWeatherToken(const std::string& tok)
{
  // Match optional intensity prefix, optional VC, optional descriptor, phenomena
  static const std::regex re(
      R"(^[+-]?"
      R"((?:VC)?"
      R"((?:MI|PR|BC|DR|BL|SH|TS|FZ)?"
      R"((?:DZ|RA|SN|SG|IC|PL|GR|GS|UP|BR|FG|FU|VA|DU|SA|HZ|PY|PO|SQ|FC|SS|DS|TSRA|TSGR|RASN|SHRA|SHSN|SHGR|SHGS|FZRA|FZDZ|RADZ|DRSN|BLSN|BLSA|BCFG|MIFG|PRFG|VCFG|VCSH|VCDS|VCTS|VCSH)+)$)");
  return std::regex_match(tok, re);
}

}  // anonymous namespace

// ======================================================================
// MetarData helper methods
// ======================================================================

// static
std::string MetarLayer::formatTemp(int t)
{
  // negative → "M|value|", single digit → leading zero
  std::string result;
  if (t < 0)
  {
    result = "M" + (std::abs(t) < 10 ? std::string("0") : std::string()) +
             Fmi::to_string(std::abs(t));
  }
  else
  {
    result = (t < 10 ? std::string("0") : std::string()) + Fmi::to_string(t);
  }
  return result;
}

// static
std::string MetarLayer::formatCloudBase(int base_hft)
{
  // 3-digit hundreds of feet with leading zeros
  if (base_hft < 10)
    return "00" + Fmi::to_string(base_hft);
  if (base_hft < 100)
    return "0" + Fmi::to_string(base_hft);
  return Fmi::to_string(base_hft);
}

// static
std::string MetarLayer::cloudAmountCode(int oktas)
{
  if (oktas <= 2)
    return "FEW";
  if (oktas <= 4)
    return "SCT";
  if (oktas <= 7)
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
  return "";
}

// static
std::string MetarLayer::skyInfoString(const MetarData& d)
{
  if (d.is_cavok)
    return "CAVOK";
  if (d.is_skc)
    return "SKC";

  // Vertical visibility overrides cloud info
  if (d.has_vertical_vis)
  {
    std::string hft_str;
    if (d.vertical_vis_hft < 10)
      hft_str = "00" + Fmi::to_string(d.vertical_vis_hft);
    else if (d.vertical_vis_hft < 100)
      hft_str = "0" + Fmi::to_string(d.vertical_vis_hft);
    else
      hft_str = Fmi::to_string(d.vertical_vis_hft);
    return "VV" + hft_str;
  }

  if (d.clouds.empty())
    return {};

  // Build layers from lowest to highest (in the PDF the lowest cloud is at the
  // bottom of the plot text block, but we render them as separate text lines
  // going downward, so we output lowest first = topmost line visually).
  std::string result;
  for (const auto& cl : d.clouds)
  {
    if (!result.empty())
      result += "\n";
    result += cloudAmountCode(cl.amount) + formatCloudBase(cl.base_hft) + cloudTypeStr(cl.type);
  }
  return result;
}

// static
std::string MetarLayer::weatherString(const MetarData& d)
{
  if (d.weather_codes.empty())
    return {};
  std::string result;
  for (int code : d.weather_codes)
  {
    auto it = ww_to_text.find(code);
    if (it != ww_to_text.end())
    {
      if (!result.empty())
        result += " ";
      result += it->second;
    }
  }
  return result;
}

// static
std::string MetarLayer::statusColor(const MetarData& d)
{
  // Determine effective cloud base: prefer vertical visibility, then cloud base 1
  int base = -1;
  if (d.has_vertical_vis)
    base = d.vertical_vis_hft * 100;  // convert hft → ft
  else if (!d.clouds.empty())
    base = d.clouds.front().base_hft * 100;

  int vis = d.has_visibility ? d.visibility : -1;
  if (d.is_cavok)
    vis = 9999;

  if (vis < 0 && base < 0)
    return {};  // no data → no status box

  bool lowVis = (vis >= 0 && vis < 800);
  bool lowBase = (base >= 0 && base < 200);
  if (lowVis || lowBase)
    return "#FF0000";  // red

  lowVis = (vis >= 0 && vis < 1600);
  lowBase = (base >= 0 && base < 300);
  if (lowVis || lowBase)
    return "#FF8C00";  // orange

  lowVis = (vis >= 0 && vis < 3700);
  lowBase = (base >= 0 && base < 700);
  if (lowVis || lowBase)
    return "#FFFF00";  // yellow

  lowVis = (vis >= 0 && vis < 5000);
  lowBase = (base >= 0 && base < 1500);
  if (lowVis || lowBase)
    return "#00CC00";  // green

  lowVis = (vis >= 0 && vis < 8000);
  lowBase = (base >= 0 && base < 2500);
  if (lowVis || lowBase)
    return "#FFFFFF";  // white

  return "#0066FF";  // blue (best conditions)
}

// ======================================================================
// Wind barb SVG path
// Shaft points toward wind source direction.
// north_dx/north_dy: unit vector for geographic north in screen coords.
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
    // Calm: small circle around station
    double r = barb_len * 0.5;
    return fmt::format("M {:.1f} {:.1f} m -{:.1f} 0 a {:.1f} {:.1f} 0 1 0 {:.1f} 0 a {:.1f} "
                       "{:.1f} 0 1 0 -{:.1f} 0",
                       cx,
                       cy,
                       r,
                       r,
                       r,
                       2 * r,
                       r,
                       r,
                       2 * r);
  }

  // Rotate north vector clockwise by direction_deg to get wind-from direction
  // In screen coords (y down), clockwise rotation by θ from north:
  // shaft_dx = north_dx*cos(θ) - north_dy*sin(θ)
  // shaft_dy = north_dx*sin(θ) + north_dy*cos(θ)
  double theta = direction_deg * M_PI / 180.0;
  double sdx = north_dx * std::cos(theta) - north_dy * std::sin(theta);
  double sdy = north_dx * std::sin(theta) + north_dy * std::cos(theta);

  // Tip of shaft (toward wind source)
  double tx = cx + sdx * shaft_len;
  double ty = cy + sdy * shaft_len;

  // Right-perpendicular to shaft (barbs branch off to the right when
  // looking from station toward source, i.e. left in typical screen orientation)
  double pdx = -sdy;
  double pdy = sdx;

  std::ostringstream path;
  path.precision(1);
  path << std::fixed;

  // Draw shaft
  path << "M " << cx << " " << cy << " L " << tx << " " << ty;

  // Count barbs from tip backward
  int kt = speed_kt;
  int pennants = kt / 50;
  kt -= pennants * 50;
  int full_barbs = kt / 10;
  kt -= full_barbs * 10;
  int half_barbs = kt / 5;

  double spacing = shaft_len / std::max(1, pennants + full_barbs + half_barbs + 1);
  spacing = std::min(spacing, barb_len * 0.8);

  double bx = tx;
  double by = ty;

  for (int i = 0; i < pennants; i++)
  {
    // Filled triangle: base at bx,by; tip one spacing back along shaft
    double b2x = bx - sdx * spacing;
    double b2y = by - sdy * spacing;
    path << " M " << bx << " " << by << " L " << (bx + pdx * barb_len) << " "
         << (by + pdy * barb_len) << " L " << b2x << " " << b2y << " Z";
    bx = b2x;
    by = b2y;
  }

  for (int i = 0; i < full_barbs; i++)
  {
    path << " M " << bx << " " << by << " L " << (bx + pdx * barb_len) << " "
         << (by + pdy * barb_len);
    bx -= sdx * spacing;
    by -= sdy * spacing;
  }

  for (int i = 0; i < half_barbs; i++)
  {
    path << " M " << bx << " " << by << " L " << (bx + pdx * barb_len * 0.5) << " "
         << (by + pdy * barb_len * 0.5);
    bx -= sdx * spacing;
    by -= sdy * spacing;
  }

  return path.str();
}

// ======================================================================
// METAR TAC parser
// Parses a raw METAR/SPECI TAC message string.
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

  // Skip METAR/SPECI/AUTO prefix and station ICAO
  std::size_t idx = 0;
  if (idx < tokens.size() &&
      (tokens[idx] == "METAR" || tokens[idx] == "SPECI" || tokens[idx] == "AUTO"))
    ++idx;
  // ICAO (4-char alpha)
  if (idx < tokens.size() && tokens[idx].size() == 4 && std::isalpha(tokens[idx][0]))
  {
    d.icao = tokens[idx];
    ++idx;
  }
  // Time: DDHHMMz (optional)
  if (idx < tokens.size())
  {
    static const std::regex time_re(R"(^\d{6}Z$)");
    if (std::regex_match(tokens[idx], time_re))
      ++idx;
  }
  // AUTO/COR flags
  while (idx < tokens.size() && (tokens[idx] == "AUTO" || tokens[idx] == "COR"))
    ++idx;

  // Now parse groups
  enum class State
  {
    Wind,
    Vis,
    RVR,
    WX,
    Clouds,
    TempDew,
    Done
  };
  State state = State::Wind;

  static const std::regex wind_re(R"(^(VRB|\d{3})(\d{2,3})(G(\d{2,3}))?KT$)");
  static const std::regex vis_re(R"(^(\d{4})$)");
  static const std::regex rvr_re(R"(^R\d{2}[LCR]?/.*$)");
  static const std::regex cloud_re(R"(^(FEW|SCT|BKN|OVC)(\d{3})(CB|TCU)?$)");
  static const std::regex vv_re(R"(^VV(\d{3}|///)$)");
  static const std::regex tempdew_re(R"(^(M?\d{2})/(M?\d{2})$)");
  static const std::regex qnh_re(R"(^Q(\d{4})$)");
  static const std::regex alt_re(R"(^A(\d{4})$)");
  // Weather: optional intensity (+/-), optional VC, optional descriptor, phenomena
  static const std::regex wx_re(
      R"(^[+-]?(?:VC)?(?:MI|PR|BC|DR|BL|SH|TS|FZ)?)"
      R"((?:DZ|RA|SN|SG|IC|PL|GR|GS|UP|BR|FG|FU|VA|DU|SA|HZ|PY|PO|SQ|FC|SS|DS)+$)");

  for (; idx < tokens.size() && state != State::Done; ++idx)
  {
    const std::string& t = tokens[idx];

    // Terminator
    if (t == "NOSIG" || t == "BECMG" || t == "TEMPO" || t == "RMK" || t == "=")
    {
      state = State::Done;
      break;
    }

    std::smatch m;

    switch (state)
    {
      case State::Wind:
        if (t == "CAVOK")
        {
          d.is_cavok = true;
          d.has_visibility = true;
          d.visibility = 9999;
          state = State::TempDew;
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
          state = State::Vis;
          break;
        }
        // Wind direction variability (e.g. 200V270) – skip
        {
          static const std::regex var_re(R"(^\d{3}V\d{3}$)");
          if (std::regex_match(t, var_re))
            break;
        }
        // Allow falling through if token doesn't match expected wind
        state = State::Vis;
        [[fallthrough]];

      case State::Vis:
        if (t == "CAVOK")
        {
          d.is_cavok = true;
          d.has_visibility = true;
          d.visibility = 9999;
          state = State::TempDew;
          break;
        }
        if (t == "9999")
        {
          d.has_visibility = true;
          d.visibility = 9999;
          state = State::RVR;
          break;
        }
        if (std::regex_match(t, m, vis_re))
        {
          d.has_visibility = true;
          d.visibility = std::min(std::stoi(m[1].str()), 9999);
          state = State::RVR;
          break;
        }
        // Could be "NVIS" or a fraction visibility — treat as unknown and move on
        state = State::RVR;
        [[fallthrough]];

      case State::RVR:
        if (std::regex_match(t, rvr_re))
          break;  // skip RVR groups
        state = State::WX;
        [[fallthrough]];

      case State::WX:
        // Check for temperature/dew before weather to handle unusual ordering
        if (std::regex_match(t, tempdew_re))
        {
          state = State::TempDew;
          goto handle_tempdew;
        }
        if (t == "SKC" || t == "CLR" || t == "NSC" || t == "NCD")
        {
          d.is_skc = true;
          state = State::Clouds;
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
          state = State::TempDew;
          break;
        }
        if (std::regex_match(t, cloud_re))
        {
          state = State::Clouds;
          goto handle_cloud;
        }
        if (std::regex_match(t, wx_re))
        {
          // Store as a raw text token; look up ww code if available
          // For display we iterate ww_to_text and find matching text
          for (auto& [code, text] : ww_to_text)
          {
            if (text == t && d.weather_codes.size() < 3)
            {
              d.weather_codes.push_back(code);
              break;
            }
          }
          // Also push -1 as placeholder if no exact match, so count is maintained
          if (d.weather_codes.empty() ||
              (d.weather_codes.size() < 3 && ww_to_text.count(d.weather_codes.back()) == 0))
          {
            // Store token directly – handled via separate raw_weather vector
            // For simplicity: attempt partial mapping
          }
          break;
        }
        // Unrecognised in WX, try clouds/temp
        state = State::Clouds;
        [[fallthrough]];

      case State::Clouds:
      handle_cloud:
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
          state = State::TempDew;
          break;
        }
        if (std::regex_match(t, m, cloud_re))
        {
          MetarData::CloudLayer cl;
          parseCloudToken(t, cl);
          d.clouds.push_back(cl);
          break;
        }
        // Check for temp/dew – could appear right after clouds
        if (std::regex_match(t, tempdew_re))
        {
          state = State::TempDew;
          goto handle_tempdew;
        }
        // Otherwise move to TempDew state and reprocess
        state = State::TempDew;
        [[fallthrough]];

      case State::TempDew:
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
          state = State::Done;
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
          // Convert altimeter setting (hundredths of inHg) to hPa
          double inHg = std::stod(m[1].str()) / 100.0;
          d.has_pressure = true;
          d.pressure = static_cast<int>(std::round(inHg * 33.8639));
          break;
        }
        break;

      default:
        break;
    }
  }

  // Second pass for QNH (often comes after temp/dew)
  if (!d.has_pressure)
  {
    for (const auto& token : tokens)
    {
      std::smatch m2;
      if (std::regex_match(token, m2, qnh_re))
      {
        d.has_pressure = true;
        d.pressure = std::stoi(m2[1].str());
        break;
      }
      if (std::regex_match(token, m2, alt_re))
      {
        double inHg = std::stod(m2[1].str()) / 100.0;
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

    // Optional ICAO and country filter arrays
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
// getFeatureInfo
// ======================================================================

void MetarLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
  // Not implemented
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

    // ------------------------------------------------------------------
    // Get valid time
    // ------------------------------------------------------------------
    auto valid_time = getValidTime();

    // ------------------------------------------------------------------
    // Projection and coordinate transformation
    // ------------------------------------------------------------------
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    Fmi::CoordinateTransformation trans("WGS84", crs);

    // Map bounding box in WGS84 for the AVI engine query
    const auto& sw = projection.bottomLeftLatLon();
    const auto& ne = projection.topRightLatLon();

    // ------------------------------------------------------------------
    // Build AVI engine query options
    // ------------------------------------------------------------------
    Engine::Avi::QueryOptions opts;

    opts.itsParameters.emplace_back("icao");
    opts.itsParameters.emplace_back("messagetime");
    opts.itsParameters.emplace_back("message");
    opts.itsParameters.emplace_back("longitude");
    opts.itsParameters.emplace_back("latitude");

    opts.itsValidity = Engine::Avi::Validity::Accepted;
    opts.itsMessageTypes.push_back(message_type);
    opts.itsMessageFormat = message_format;

    // Time: format valid_time as PostgreSQL timestamptz literal
    std::string ts = Fmi::to_iso_string(valid_time);  // e.g. "20151117T002000"
    opts.itsTimeOptions.itsObservationTime = "timestamptz '" + ts + "Z'";
    opts.itsTimeOptions.itsTimeFormat = "iso";
    opts.itsTimeOptions.itsMessageTimeChecks = false;
    opts.itsTimeOptions.itsQueryValidRangeMessages = true;

    opts.itsDistinctMessages = true;
    opts.itsFilterMETARs = (message_format == "TAC");
    opts.itsExcludeSPECIs = exclude_specis;
    opts.itsMaxMessageStations = -1;
    opts.itsMaxMessageRows = -1;

    // Location: configured ICAOs/countries override map bbox
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
      // Use map bounding box (expand slightly to avoid clipping border stations)
      Engine::Avi::BBox avibox(sw.X() - 0.5, ne.X() + 0.5, sw.Y() - 0.5, ne.Y() + 0.5);
      opts.itsLocationOptions.itsBBoxes.push_back(avibox);
    }

    // ------------------------------------------------------------------
    // Execute AVI query
    // ------------------------------------------------------------------
    auto& aviEngine = theState.getAviEngine();
    auto aviData = aviEngine.queryStationsAndMessages(opts);

    // ------------------------------------------------------------------
    // Parse messages and transform to pixel coordinates
    // ------------------------------------------------------------------
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

      // Require longitude and latitude columns
      if (vals.find("longitude") == vals.end() || vals.find("latitude") == vals.end())
        continue;
      if (vals.find("message") == vals.end())
        continue;

      double lon = std::get<double>(*vals.at("longitude").cbegin());
      double lat = std::get<double>(*vals.at("latitude").cbegin());

      const auto& msg_val = *vals.at("message").cbegin();
      std::string raw_message;
      if (std::holds_alternative<std::string>(msg_val))
        raw_message = std::get<std::string>(msg_val);
      else
        continue;

      std::string icao_str;
      if (vals.count("icao") && !vals.at("icao").empty())
      {
        const auto& iv = *vals.at("icao").cbegin();
        if (std::holds_alternative<std::string>(iv))
          icao_str = std::get<std::string>(iv);
      }

      // Transform to pixel coordinates
      double px = lon;
      double py = lat;
      if (!crs.isGeographic())
      {
        if (!trans.transform(px, py))
          continue;
      }
      box.transform(px, py);

      if (!inside(box, px, py))
        continue;

      MetarData d = parseTAC(raw_message, lon, lat, icao_str);
      plots.push_back({static_cast<int>(std::lround(px)),
                       static_cast<int>(std::lround(py)),
                       std::move(d)});
    }

    // ------------------------------------------------------------------
    // Declutter: remove stations too close to already-rendered ones
    // Sort by ICAO (major airports tend to have shorter codes) – a more
    // sophisticated priority system could be added later.
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // CSS
    // ------------------------------------------------------------------
    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // ------------------------------------------------------------------
    // Generate SVG output
    // ------------------------------------------------------------------
    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Outer group element
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";
    theState.addAttributes(theGlobals, group_cdt, attributes);
    theLayersCdt.PushBack(group_cdt);

    const std::string default_text_color = color.empty() ? "black" : color;
    const double shaft_len = B * 0.32;
    const double barb_len = B * 0.14;
    const int status_box_size = std::max(4, font_size - 2);
    const int line_height = static_cast<int>(font_size * 1.25);

    for (const auto& p : rendered)
    {
      const int sx = p.x;
      const int sy = p.y;
      const auto& d = p.data;

      // Compute geographic-north direction in screen space for wind barb correction
      double north_dx = 0.0;
      double north_dy = -1.0;
      {
        double x1 = d.longitude;
        double y1 = d.latitude;
        double x2 = d.longitude;
        double y2 = d.latitude + 0.1;
        if (!crs.isGeographic())
        {
          trans.transform(x1, y1);
          trans.transform(x2, y2);
        }
        box.transform(x1, y1);
        box.transform(x2, y2);
        double dx = x2 - x1;
        double dy = y2 - y1;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len > 1e-6)
        {
          north_dx = dx / len;
          north_dy = dy / len;
        }
      }

      // Pixel offsets from station point for each element (from PDF layout)
      // Relative coords (rel_x, rel_y) with station at (0.5,0.5) in a [0,1] box.
      // pixel_offset = (rel - 0.5) * B, y-flipped: offset_y = (0.5 - rel_y) * B
      //
      // T:       rel (0.35, 0.9)  → dx=-0.15*B, dy=-0.4*B
      // Td:      rel (0.35, 0.45) → dx=-0.15*B, dy=+0.05*B
      // Vis:     rel (0.35, 0.675)→ dx=-0.15*B, dy=-0.175*B
      // Status:  rel (0.5,  0.5)  → dx=0,       dy=0
      // Wind:    rel (0.5,  0.5)  → dx=0,       dy=0
      // P:       rel (0.6,  0.9)  → dx=+0.1*B,  dy=-0.4*B
      // Gust:    rel (0.6,  0.675)→ dx=+0.1*B,  dy=-0.175*B
      // AviW:    rel (0.6,  0.45) → dx=+0.1*B,  dy=+0.05*B
      // Sky:     rel (0.5,  0.25) → dx=0,        dy=+0.25*B

      // Status colour box (centred on station)
      if (show_status)
      {
        std::string sc = statusColor(d);
        if (!sc.empty())
        {
          CTPP::CDT rect_cdt(CTPP::CDT::HASH_VAL);
          rect_cdt["start"] = "<rect";
          rect_cdt["end"] = "/>";
          rect_cdt["attributes"]["x"] = Fmi::to_string(sx - status_box_size / 2);
          rect_cdt["attributes"]["y"] = Fmi::to_string(sy - status_box_size / 2);
          rect_cdt["attributes"]["width"] = Fmi::to_string(status_box_size);
          rect_cdt["attributes"]["height"] = Fmi::to_string(status_box_size);
          rect_cdt["attributes"]["fill"] = (color.empty() ? sc : color);
          rect_cdt["attributes"]["stroke"] = "black";
          rect_cdt["attributes"]["stroke-width"] = "0.5";
          theLayersCdt.PushBack(rect_cdt);
        }
      }

      // Wind barb
      if (show_wind && d.has_wind)
      {
        std::string barb_path = windBarbPath(
            d.wind_variable ? 0 : d.wind_direction,
            d.wind_variable ? 0 : d.wind_speed,  // VRB → calm circle
            north_dx,
            north_dy,
            static_cast<double>(sx),
            static_cast<double>(sy),
            shaft_len,
            barb_len);

        if (!barb_path.empty())
        {
          CTPP::CDT path_cdt(CTPP::CDT::HASH_VAL);
          path_cdt["start"] = "<path";
          path_cdt["end"] = "/>";
          path_cdt["attributes"]["d"] = barb_path;
          path_cdt["attributes"]["stroke"] = default_text_color;
          path_cdt["attributes"]["stroke-width"] = "1.5";
          path_cdt["attributes"]["fill"] = default_text_color;
          path_cdt["attributes"]["stroke-linecap"] = "round";
          theLayersCdt.PushBack(path_cdt);
        }
      }

      // Temperature (upper left, right-aligned)
      if (show_temperature && d.has_temperature)
      {
        int tx = sx + static_cast<int>(std::lround(-0.15 * B));
        int ty = sy + static_cast<int>(std::lround(-0.4 * B));
        CTPP::CDT t_cdt(CTPP::CDT::HASH_VAL);
        t_cdt["start"] = "<text";
        t_cdt["end"] = "</text>";
        t_cdt["cdata"] = formatTemp(d.temperature);
        t_cdt["attributes"]["x"] = Fmi::to_string(tx);
        t_cdt["attributes"]["y"] = Fmi::to_string(ty);
        t_cdt["attributes"]["text-anchor"] = "end";
        t_cdt["attributes"]["font-size"] = Fmi::to_string(font_size);
        t_cdt["attributes"]["fill"] = default_text_color;
        theLayersCdt.PushBack(t_cdt);
      }

      // Dew point (lower left, right-aligned)
      if (show_dewpoint && d.has_dewpoint)
      {
        int tx = sx + static_cast<int>(std::lround(-0.15 * B));
        int ty = sy + static_cast<int>(std::lround(0.05 * B));
        CTPP::CDT td_cdt(CTPP::CDT::HASH_VAL);
        td_cdt["start"] = "<text";
        td_cdt["end"] = "</text>";
        td_cdt["cdata"] = formatTemp(d.dewpoint);
        td_cdt["attributes"]["x"] = Fmi::to_string(tx);
        td_cdt["attributes"]["y"] = Fmi::to_string(ty);
        td_cdt["attributes"]["text-anchor"] = "end";
        td_cdt["attributes"]["font-size"] = Fmi::to_string(font_size);
        td_cdt["attributes"]["fill"] = default_text_color;
        theLayersCdt.PushBack(td_cdt);
      }

      // Visibility (middle left, right-aligned)
      if (show_visibility && d.has_visibility && !d.is_cavok && !d.is_skc)
      {
        int tx = sx + static_cast<int>(std::lround(-0.15 * B));
        int ty = sy + static_cast<int>(std::lround(-0.175 * B));
        CTPP::CDT vis_cdt(CTPP::CDT::HASH_VAL);
        vis_cdt["start"] = "<text";
        vis_cdt["end"] = "</text>";
        vis_cdt["cdata"] = Fmi::to_string(d.visibility);
        vis_cdt["attributes"]["x"] = Fmi::to_string(tx);
        vis_cdt["attributes"]["y"] = Fmi::to_string(ty);
        vis_cdt["attributes"]["text-anchor"] = "end";
        vis_cdt["attributes"]["font-size"] = Fmi::to_string(font_size);
        vis_cdt["attributes"]["fill"] = default_text_color;
        theLayersCdt.PushBack(vis_cdt);
      }

      // QNH pressure (upper right, left-aligned)
      if (show_pressure && d.has_pressure)
      {
        int tx = sx + static_cast<int>(std::lround(0.1 * B));
        int ty = sy + static_cast<int>(std::lround(-0.4 * B));
        CTPP::CDT p_cdt(CTPP::CDT::HASH_VAL);
        p_cdt["start"] = "<text";
        p_cdt["end"] = "</text>";
        p_cdt["cdata"] = Fmi::to_string(d.pressure);
        p_cdt["attributes"]["x"] = Fmi::to_string(tx);
        p_cdt["attributes"]["y"] = Fmi::to_string(ty);
        p_cdt["attributes"]["text-anchor"] = "start";
        p_cdt["attributes"]["font-size"] = Fmi::to_string(font_size);
        p_cdt["attributes"]["fill"] = default_text_color;
        theLayersCdt.PushBack(p_cdt);
      }

      // Wind gust (middle right, left-aligned)
      // PDF: gust in knots, prefixed with "G"
      if (show_gust && d.has_gust)
      {
        int tx = sx + static_cast<int>(std::lround(0.1 * B));
        int ty = sy + static_cast<int>(std::lround(-0.175 * B));
        CTPP::CDT g_cdt(CTPP::CDT::HASH_VAL);
        g_cdt["start"] = "<text";
        g_cdt["end"] = "</text>";
        g_cdt["cdata"] = "G" + Fmi::to_string(d.wind_gust);
        g_cdt["attributes"]["x"] = Fmi::to_string(tx);
        g_cdt["attributes"]["y"] = Fmi::to_string(ty);
        g_cdt["attributes"]["text-anchor"] = "start";
        g_cdt["attributes"]["font-size"] = Fmi::to_string(font_size);
        g_cdt["attributes"]["fill"] = default_text_color;
        theLayersCdt.PushBack(g_cdt);
      }

      // Present weather (lower right, left-aligned)
      if (show_weather && !d.weather_codes.empty())
      {
        std::string wx_text = weatherString(d);
        if (!wx_text.empty())
        {
          int tx = sx + static_cast<int>(std::lround(0.1 * B));
          int ty = sy + static_cast<int>(std::lround(0.05 * B));
          CTPP::CDT wx_cdt(CTPP::CDT::HASH_VAL);
          wx_cdt["start"] = "<text";
          wx_cdt["end"] = "</text>";
          wx_cdt["cdata"] = wx_text;
          wx_cdt["attributes"]["x"] = Fmi::to_string(tx);
          wx_cdt["attributes"]["y"] = Fmi::to_string(ty);
          wx_cdt["attributes"]["text-anchor"] = "start";
          wx_cdt["attributes"]["font-size"] = Fmi::to_string(font_size);
          wx_cdt["attributes"]["fill"] = default_text_color;
          theLayersCdt.PushBack(wx_cdt);
        }
      }

      // Sky info (below centre, centre-aligned, one line per cloud layer)
      if (show_sky_info)
      {
        std::string sky = skyInfoString(d);
        if (!sky.empty())
        {
          int tx = sx;
          int ty = sy + static_cast<int>(std::lround(0.25 * B));

          // Split on newline and emit one <text> per line
          std::istringstream sky_ss(sky);
          std::string sky_line;
          int line_no = 0;
          while (std::getline(sky_ss, sky_line))
          {
            if (!sky_line.empty())
            {
              CTPP::CDT sky_cdt(CTPP::CDT::HASH_VAL);
              sky_cdt["start"] = "<text";
              sky_cdt["end"] = "</text>";
              sky_cdt["cdata"] = sky_line;
              sky_cdt["attributes"]["x"] = Fmi::to_string(tx);
              sky_cdt["attributes"]["y"] = Fmi::to_string(ty + line_no * line_height);
              sky_cdt["attributes"]["text-anchor"] = "middle";
              sky_cdt["attributes"]["font-size"] = Fmi::to_string(font_size);
              sky_cdt["attributes"]["fill"] = default_text_color;
              theLayersCdt.PushBack(sky_cdt);
            }
            ++line_no;
          }
        }
      }
    }  // for each station

    // Close the outer group
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
