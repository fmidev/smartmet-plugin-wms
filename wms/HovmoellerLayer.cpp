// ======================================================================
/*!
 * \brief Hovmöller diagram layer implementation
 */
// ======================================================================

#include "HovmoellerLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Isoband.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/querydata/Q.h>
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiGlobals.h>
#include <newbase/NFmiPoint.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <sstream>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{

// Return the CSS class for a data value given ordered isoband definitions.
// The first band whose [lolimit, hilimit) interval contains the value wins.
// A band with neither limit is the missing-value catch-all.
std::string isoband_class(float value, const std::vector<Isoband>& isobands)
{
  const bool missing = (value == kFloatMissing);
  for (const auto& band : isobands)
  {
    if (!band.lolimit && !band.hilimit)
    {
      if (missing)
        return band.attributes.value("class");
      continue;
    }
    if (missing)
      continue;
    bool lo_ok = !band.lolimit || (value >= static_cast<float>(*band.lolimit));
    bool hi_ok = !band.hilimit || (value < static_cast<float>(*band.hilimit));
    if (lo_ok && hi_ok)
      return band.attributes.value("class");
  }
  return {};
}

// Emit one SVG rect; width/height are inflated by 0.5px to close sub-pixel gaps.
void emit_rect(std::ostringstream& out,
               double x,
               double y,
               double cell_w,
               double cell_h,
               const std::string& css_class)
{
  out << fmt::format(
      "<rect x=\"{:.2f}\" y=\"{:.2f}\" width=\"{:.2f}\" height=\"{:.2f}\" class=\"{}\"/>",
      x, y, cell_w + 0.5, cell_h + 0.5, css_class);
}

}  // anonymous namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void HovmoellerLayer::init(Json::Value& theJson,
                           const State& theState,
                           const Config& theConfig,
                           const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "HovmoellerLayer JSON is not an object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // parameter and level are parsed by Properties::init via Layer::init;
    // we only need to pick up the parameter name explicitly here.
    JsonTools::remove_string(paraminfo.parameter, theJson, "parameter");

    JsonTools::remove_string(direction, theJson, "direction");

    if (direction != "time_lon" && direction != "time_lat" && direction != "time_level")
      throw Fmi::Exception(BCP,
          "HovmoellerLayer: direction must be time_lon, time_lat or time_level, got '" +
          direction + "'");

    JsonTools::remove_double(latitude, theJson, "latitude");
    JsonTools::remove_double(longitude, theJson, "longitude");
    JsonTools::remove_double(lon_min, theJson, "lon_min");
    JsonTools::remove_double(lon_max, theJson, "lon_max");
    JsonTools::remove_double(lat_min, theJson, "lat_min");
    JsonTools::remove_double(lat_max, theJson, "lat_max");
    JsonTools::remove_int(nx, theJson, "nx");

    if (nx < 2)
      throw Fmi::Exception(BCP, "HovmoellerLayer: nx must be >= 2");

    auto json = JsonTools::remove(theJson, "isobands");
    if (!json.isNull())
      JsonTools::extract_array("isobands", isobands, json, theConfig);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "HovmoellerLayer::init failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate SVG content into the CTPP CDT
 */
// ----------------------------------------------------------------------

void HovmoellerLayer::generate(CTPP::CDT& theGlobals,
                               CTPP::CDT& theLayersCdt,
                               State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "HovmoellerLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // ---- acquire data ----

    auto q = getModel(theState);
    if (!q)
      throw Fmi::Exception(BCP, "HovmoellerLayer: no querydata available");
    if (!q->isGrid())
      throw Fmi::Exception(BCP, "HovmoellerLayer: querydata must be gridded");
    if (paraminfo.parameter.empty())
      throw Fmi::Exception(BCP, "HovmoellerLayer: parameter not set");

    auto param = TS::ParameterFactory::instance().parse(paraminfo.parameter);

    // Select pressure level for the two spatial-slice directions
    if (direction != "time_level")
    {
      if (!q->firstLevel())
        throw Fmi::Exception(BCP, "HovmoellerLayer: unable to reset levels");
      if (paraminfo.level)
      {
        if (!q->selectLevel(*paraminfo.level))
          throw Fmi::Exception(BCP,
              "HovmoellerLayer: level " + Fmi::to_string(*paraminfo.level) + " not available");
      }
    }

    if (!q->param(param.number()))
      throw Fmi::Exception(BCP,
          "HovmoellerLayer: parameter '" + paraminfo.parameter + "' not available in data");

    // ---- valid times ----

    auto valid_times_ptr = q->validTimes();
    if (!valid_times_ptr || valid_times_ptr->empty())
      throw Fmi::Exception(BCP, "HovmoellerLayer: no valid times in querydata");
    const auto& valid_times = *valid_times_ptr;
    const int n_times = static_cast<int>(valid_times.size());

    // ---- canvas dimensions from the product globals ----

    const int canvas_w = static_cast<int>(theGlobals["width"].GetInt());
    const int canvas_h = static_cast<int>(theGlobals["height"].GetInt());
    if (canvas_w <= 0 || canvas_h <= 0)
      throw Fmi::Exception(BCP,
          "HovmoellerLayer: canvas size not set – add xsize/ysize to the product projection");

    // ---- render ----

    std::ostringstream svg;

    if (direction == "time_lon" || direction == "time_lat")
    {
      const bool time_lon = (direction == "time_lon");
      const double space_min = time_lon ? lon_min : lat_min;
      const double space_max = time_lon ? lon_max : lat_max;
      const double fixed     = time_lon ? latitude : longitude;

      const double cell_w = static_cast<double>(canvas_w) / nx;
      const double cell_h = static_cast<double>(canvas_h) / n_times;

      int ti = 0;
      for (const auto& t : valid_times)
      {
        for (int si = 0; si < nx; ++si)
        {
          const double space = space_min + (space_max - space_min) * si / (nx - 1);
          const double lon   = time_lon ? space : fixed;
          const double lat   = time_lon ? fixed : space;

          const float value = q->interpolate(NFmiPoint(lon, lat), t, 180);
          const std::string cls = isoband_class(value, isobands);
          if (!cls.empty())
            emit_rect(svg, si * cell_w, ti * cell_h, cell_w, cell_h, cls);
        }
        ++ti;
      }
    }
    else  // time_level
    {
      // Collect all available pressure levels in data order
      std::vector<double> levels;
      for (q->resetLevel(); q->nextLevel();)
        levels.push_back(q->levelValue());
      const int n_levels = static_cast<int>(levels.size());
      if (n_levels == 0)
        throw Fmi::Exception(BCP, "HovmoellerLayer: no levels in data");

      const double cell_w = static_cast<double>(canvas_w) / n_levels;
      const double cell_h = static_cast<double>(canvas_h) / n_times;

      int ti = 0;
      for (const auto& t : valid_times)
      {
        for (int li = 0; li < n_levels; ++li)
        {
          q->selectLevel(levels[li]);
          q->param(param.number());
          const float value = q->interpolate(NFmiPoint(longitude, latitude), t, 180);
          const std::string cls = isoband_class(value, isobands);
          if (!cls.empty())
            emit_rect(svg, li * cell_w, ti * cell_h, cell_w, cell_h, cls);
        }
        ++ti;
      }
    }

    // ---- CSS ----

    if (css)
      theGlobals["css"].PushBack(theState.getStyle(*css));

    // ---- assemble CDT layer entry ----

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    theState.addAttributes(theGlobals, group_cdt, attributes);
    group_cdt["cdata"] = svg.str();

    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "HovmoellerLayer::generate failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
