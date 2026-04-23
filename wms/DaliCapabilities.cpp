// ======================================================================
/*!
 * \brief JSON catalog endpoint for Dali products
 */
// ======================================================================

#include "DaliCapabilities.h"
#include "Config.h"
#include "Plugin.h"
#include "State.h"
#include <engines/querydata/Engine.h>
#include <fmt/format.h>
#include <json/json.h>
#include <json/writer.h>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <macgyver/TimeFormatter.h>
#include <spine/Convenience.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// Read a file fully into a string. Returns empty string on failure; errors are
// swallowed by the caller (a malformed product JSON should not break the catalog).

std::string slurp(const std::filesystem::path& thePath)
{
  std::ifstream in(thePath);
  if (!in)
    return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Format a DateTime as ISO8601 UTC ("YYYY-MM-DDTHH:MM:SSZ"). Returns empty for NOT_A_DATE_TIME.

std::string to_iso(const Fmi::DateTime& t)
{
  if (t.is_not_a_date_time())
    return {};
  static const auto formatter =
      std::shared_ptr<Fmi::TimeFormatter>(Fmi::TimeFormatter::create("iso"));
  return formatter->format(t);
}

// Detect a uniform timestep and return "start/end/PTnH" style ISO8601 interval,
// or an empty string if the list is not uniform. Minimum interval is 1 minute.

std::string detect_interval(const std::vector<Fmi::DateTime>& times)
{
  if (times.size() < 3)
    return {};

  const auto step = times[1] - times[0];
  if (step.total_seconds() <= 0)
    return {};

  for (std::size_t i = 2; i < times.size(); ++i)
  {
    if (times[i] - times[i - 1] != step)
      return {};
  }

  const long secs = step.total_seconds();
  std::string resolution;
  if (secs % 3600 == 0)
    resolution = fmt::format("PT{}H", secs / 3600);
  else if (secs % 60 == 0)
    resolution = fmt::format("PT{}M", secs / 60);
  else
    resolution = fmt::format("PT{}S", secs);

  return to_iso(times.front()) + "/" + to_iso(times.back()) + "/" + resolution;
}

// Produce a time JSON node from a producer, using the QEngine. The node has
// "default" (latest valid time) and "values" (comma-separated ISO8601 or a
// start/end/step interval). Returns Json::nullValue if the producer is unknown
// or has no times.

Json::Value time_node(const Engine::Querydata::Engine& theQEngine,
                      const Engine::Querydata::Producer& theProducer)
{
  try
  {
    if (!theQEngine.hasProducer(theProducer))
      return Json::nullValue;

    std::vector<Fmi::DateTime> times;
    Fmi::DateTime latest_origin = Fmi::DateTime::NOT_A_DATE_TIME;

    auto origintimes = theQEngine.origintimes(theProducer);
    if (origintimes.empty())
    {
      // Multifile producer: single sorted list of valid times across the whole archive.
      auto q = theQEngine.get(theProducer);
      auto vt = q->validTimes();
      if (vt)
        times.assign(vt->begin(), vt->end());
    }
    else
    {
      // Use the most recent origin time's valid times (the usual WMS
      // convention — one dataset, current forecast).
      latest_origin = *origintimes.rbegin();
      auto q = theQEngine.get(theProducer, latest_origin);
      auto vt = q->validTimes();
      if (vt)
        times.assign(vt->begin(), vt->end());
    }

    if (times.empty())
      return Json::nullValue;

    Json::Value node;
    node["default"] = to_iso(times.back());
    if (!latest_origin.is_not_a_date_time())
      node["origintime"] = to_iso(latest_origin);

    const auto interval = detect_interval(times);
    if (!interval.empty())
    {
      node["values"] = interval;
    }
    else
    {
      std::string joined;
      joined.reserve(times.size() * 21);
      bool first = true;
      for (const auto& t : times)
      {
        if (!first)
          joined.push_back(',');
        joined += to_iso(t);
        first = false;
      }
      node["values"] = joined;
    }
    return node;
  }
  catch (...)
  {
    // A producer lookup failure must not fail the whole catalog.
    return Json::nullValue;
  }
}

// Parse a product JSON file (raw, no reference expansion) and build a catalog entry.
// 'theRelative' is the product name relative to the customer's products/ directory,
// without the .json extension, e.g. "aurausmalli/roadcondition".

Json::Value product_entry(const std::filesystem::path& theFile,
                          const std::string& theCustomer,
                          const std::string& theRelative,
                          const Engine::Querydata::Engine& theQEngine)
{
  Json::Value entry;
  entry["name"] = theRelative;

  Json::Value path(Json::arrayValue);
  path.append(theCustomer);
  std::string remaining = theRelative;
  while (true)
  {
    auto slash = remaining.find('/');
    if (slash == std::string::npos)
    {
      path.append(remaining);
      break;
    }
    path.append(remaining.substr(0, slash));
    remaining = remaining.substr(slash + 1);
  }
  entry["path"] = path;

  const std::string text = slurp(theFile);
  if (text.empty())
  {
    entry["error"] = "unreadable";
    return entry;
  }

  Json::CharReaderBuilder rb;
  Json::Value json;
  std::string errors;
  std::unique_ptr<Json::CharReader> reader(rb.newCharReader());
  if (!reader->parse(text.data(), text.data() + text.size(), &json, &errors))
  {
    entry["error"] = "parse_error";
    return entry;
  }

  // Pass through strictly user-visible metadata. We do not resolve "json:"
  // references here — a catalog entry only needs the top-level product fields.
  if (json.isMember("title"))
    entry["title"] = json["title"];
  if (json.isMember("abstract"))
    entry["abstract"] = json["abstract"];

  std::string producer;
  if (json.isMember("producer") && json["producer"].isString())
  {
    producer = json["producer"].asString();
    entry["producer"] = producer;
  }

  if (json.isMember("source"))
    entry["source"] = json["source"];

  // Output image dimensions. Dali products may specify them at product level or
  // inside projection.xsize/ysize (the latter is the actual raster size; the
  // former is the full SVG canvas including chrome). We expose both when useful.
  Json::Value image(Json::objectValue);
  if (json.isMember("width") && json["width"].isIntegral())
    image["width"] = json["width"];
  if (json.isMember("height") && json["height"].isIntegral())
    image["height"] = json["height"];

  if (json.isMember("projection") && json["projection"].isObject())
  {
    const auto& proj = json["projection"];
    Json::Value projection(Json::objectValue);
    for (const auto& key : {"crs", "bboxcrs", "xsize", "ysize", "x1", "y1", "x2", "y2",
                            "cx", "cy", "resolution"})
    {
      if (proj.isMember(key))
        projection[key] = proj[key];
    }
    if (!projection.empty())
      entry["projection"] = projection;

    // Fall back to projection.xsize/ysize if top-level width/height are absent.
    if (!image.isMember("width") && proj.isMember("xsize"))
      image["width"] = proj["xsize"];
    if (!image.isMember("height") && proj.isMember("ysize"))
      image["height"] = proj["ysize"];
  }

  // Default output type is SVG (matches daliQuery default) but most browser
  // consumers will pass &type=png explicitly.
  if (json.isMember("type"))
    image["format"] = json["type"];
  if (!image.empty())
    entry["image"] = image;

  // Time dimension from the Querydata engine if the producer is known.
  if (!producer.empty())
  {
    auto tn = time_node(theQEngine, producer);
    if (!tn.isNull())
      entry["time"] = tn;
  }

  // URL template clients can expand. Keep type parameterless so clients pick.
  entry["url_template"] =
      fmt::format("/dali?customer={}&product={}&time={{time}}", theCustomer, theRelative);

  return entry;
}

}  // namespace

DaliCapabilities::DaliCapabilities(const Plugin& thePlugin) : itsPlugin(thePlugin) {}

std::string DaliCapabilities::generate(const Spine::HTTP::Request& theRequest,
                                       State& theState) const
{
  try
  {
    const auto& config = itsPlugin.getConfig();
    const std::string root = config.rootDirectory(false);  // Dali = !wms
    const std::filesystem::path customers_root(root + "/customers");

    // Optional customer filter. If absent, list all customers.
    const auto customer_param = theRequest.getParameter("customer");

    Json::Value out(Json::objectValue);
    out["service"] = "Dali";
    out["version"] = "1.0.0";

    Json::Value customers(Json::arrayValue);

    if (!std::filesystem::exists(customers_root) ||
        !std::filesystem::is_directory(customers_root))
    {
      out["customers"] = customers;  // empty
      Json::StreamWriterBuilder wb;
      wb["indentation"] = "  ";
      return Json::writeString(wb, out);
    }

    // Sort customer directories so the catalog is deterministic across runs
    // (std::filesystem::directory_iterator gives no order guarantee).
    std::vector<std::filesystem::path> customer_dirs;
    for (const auto& customer_dir : std::filesystem::directory_iterator(customers_root))
    {
      if (customer_dir.is_directory())
        customer_dirs.push_back(customer_dir.path());
    }
    std::sort(customer_dirs.begin(), customer_dirs.end());

    for (const auto& customer_path : customer_dirs)
    {
      const std::string customer = customer_path.filename().string();
      if (customer_param && !customer_param->empty() && *customer_param != customer)
        continue;

      const std::filesystem::path products_root = customer_path / "products";
      if (!std::filesystem::exists(products_root))
        continue;

      // Collect and sort .json paths under products/ so output order is stable.
      std::vector<std::filesystem::path> product_files;
      for (const auto& entry :
           std::filesystem::recursive_directory_iterator(products_root))
      {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
          product_files.push_back(entry.path());
      }
      std::sort(product_files.begin(), product_files.end());

      Json::Value products(Json::arrayValue);
      for (const auto& product_path : product_files)
      {
        // Relative path without .json -> product name (e.g. "aurausmalli/roadcondition").
        auto rel = std::filesystem::relative(product_path, products_root);
        std::string relname = rel.generic_string();
        if (relname.size() > 5 && relname.substr(relname.size() - 5) == ".json")
          relname.resize(relname.size() - 5);

        try
        {
          products.append(product_entry(product_path,
                                        customer,
                                        relname,
                                        itsPlugin.getQEngine()));
        }
        catch (...)
        {
          // Per-product failures must not abort the catalog.
          Json::Value err;
          err["name"] = relname;
          err["error"] = "exception";
          products.append(err);
        }
      }

      Json::Value c;
      c["name"] = customer;
      c["products"] = products;
      customers.append(c);
    }

    out["customers"] = customers;

    theState.updateExpirationTime(Fmi::SecondClock::universal_time() + Fmi::Seconds(60));

    Json::StreamWriterBuilder wb;
    wb["indentation"] = "  ";
    return Json::writeString(wb, out);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Dali GetCapabilities generation failed");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
