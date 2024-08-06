// ======================================================================
/*!
 * \brief SmartMet Dali plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "CaseInsensitiveComparator.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Mime.h"
#include "ParameterInfo.h"
#include "Product.h"
#include "State.h"
#include "StyleSelection.h"
#include "TextUtility.h"
#include "WMSConfig.h"
#include "WMSGetCapabilities.h"
#include "WMSGetLegendGraphic.h"
#include "WMSGetMap.h"
#include "WMSRequestType.h"
#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif
#include <boost/timer/timer.hpp>
#include <boost/utility.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <giza/Svg.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <spine/HostInfo.h>
#include <spine/Json.h>
#include <spine/SmartMet.h>
#include <memory>
#include <stdexcept>

using namespace boost::placeholders;

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
Json::CharReaderBuilder charreaderbuilder;

// ----------------------------------------------------------------------
/*!
 * \brief Validate name for inclusion
 *
 * A valid name does not lead upwards in a path if inserted into a path. If the name looks valid, we
 * return it so that we do not have to use if statements when this check is done. If the name does
 * not look valid, we throw.
 */
// ----------------------------------------------------------------------

const std::string &check_attack(const std::string &theName)
{
  try
  {
    if (theName.find("./") == std::string::npos)
      return theName;

    throw Fmi::Exception(
        BCP, "Attack IRI detected, relative paths upwards are not safe: '" + theName + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Keys accepted by Product and Properties classes:

const std::set<std::string, Spine::HTTP::ParamMap::key_compare> allowed_keys = {
    "attributes", "clip",     "defs",         "forecastNumber", "forecastType",
    "geometryId", "height",   "interval_end", "interval_start", "language",
    "level",      "level",    "levelId",      "levelid",        "margin",
    "origintime", "png",      "producer",     "projection",     "source",
    "source",     "svg_tmpl", "time",         "time_offset",    "timestep",
    "title",      "type",     "tz",           "views",          "width",
    "xmargin",    "ymargin"};

void check_remaining_dali_json(Json::Value &json, const std::string &name)
{
  std::vector<std::string> deletions{"abstract", "refs", "styles"};

  for (const auto &delete_name : deletions)
    static_cast<void>(JsonTools::remove(json, delete_name));

  if (!json.empty())
  {
    Json::StyledWriter writer;
    std::cout << fmt::format("{} Remaining Dali json for product {}:\n{}\n",
                             Spine::log_time_str(),
                             name,
                             writer.write(json))
              << std::flush;
  }
}

}  // namespace

// Keep only acceptable querystring replacements (allowed keys or names with dots)

Spine::HTTP::ParamMap Dali::Plugin::extractValidParameters(const Spine::HTTP::ParamMap &theParams)
{
  Spine::HTTP::ParamMap ret;

  for (const auto &name_value : theParams)
  {
    const auto &name = name_value.first;
    bool has_dot = (name.find('.') != std::string::npos);
    if (has_dot || allowed_keys.find(name) != allowed_keys.end())
      ret.insert(name_value);
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \print Information on product parameters
 */
// ----------------------------------------------------------------------

void Dali::Plugin::print(const ParameterInfos &infos)
{
  if (infos.empty())
  {
    std::cout << "No grid parameters used in the product\n";
  }
  else
  {
    const std::string missing = "?";
    std::cout << "Parameter information:\n";
    for (const auto &info : infos)
    {
      std::cout << fmt::format("Parameter={} Producer={} Level={}\n",
                               info.parameter,
                               (info.producer ? *info.producer : missing),
                               (info.level ? Fmi::to_string(*info.level) : missing));
    }
  }
  std::cout << std::flush;
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a Dali query
 */
// ----------------------------------------------------------------------

void Dali::Plugin::daliQuery(Spine::Reactor & /* theReactor */,
                             State &theState,
                             const Spine::HTTP::Request &theRequest,
                             Spine::HTTP::Response &theResponse)
{
  try
  {
    if (checkRequest(theRequest, theResponse, false))
    {
      return;
    }

    int width = Spine::optional_int(theRequest.getParameter("width"), 1000);
    int height = Spine::optional_int(theRequest.getParameter("height"), 1000);

    if (static_cast<double>(width) * static_cast<double>(height) > itsConfig.maxImageSize())
    {
      throw Fmi::Exception(BCP, "Too large image requested")
          .addParameter("limit", Fmi::to_string(itsConfig.maxImageSize()));
    }

    if (width < 20 || width > 10000)
      throw Fmi::Exception(BCP, "Invalid 'width' value '" + Fmi::to_string(width) + "'!");

    if (height < 20 || height > 10000)
      throw Fmi::Exception(BCP, "Invalid 'height' value '" + Fmi::to_string(height) + "'!");

    // Establish debugging related variables

    const bool print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);

    const bool print_json = Spine::optional_bool(theRequest.getParameter("printjson"), false);

    const int json_stage = Spine::optional_int(theRequest.getParameter("stage"), 0);

    const bool print_params = Spine::optional_bool(theRequest.getParameter("printparams"), false);

    const bool usetimer = Spine::optional_bool(theRequest.getParameter("timer"), false);

    // Define the customer and image format

    theState.setCustomer(
        Spine::optional_string(theRequest.getParameter("customer"), itsConfig.defaultCustomer()));

    if (theState.getCustomer().empty())
      throw Fmi::Exception(BCP, "Customer setting is empty");

    // Get the product configuration

    std::string product_name = Spine::required_string(
        theRequest.getParameter("product"), "Product configuration option 'product' not given");

    theState.setName(theState.getCustomer() + "/" + product_name);

    auto json = getProductJson(theRequest, theState, product_name, json_stage);

    // Debugging

    if (print_json)
    {
      Json::StyledWriter writer;
      std::cout << fmt::format("Expanded {} Spine::Json:\n{}\n", product_name, writer.write(json));
    }

    // Special product for reading configuration files

    auto fmt = Spine::optional_string(theRequest.getParameter("type"), "svg");
    theState.setType(fmt);

    // Trivial error checks done first for speed

    if (!boost::iequals(fmt, "xml") && !boost::iequals(fmt, "svg") && !boost::iequals(fmt, "png") &&
        !boost::iequals(fmt, "pdf") && !boost::iequals(fmt, "ps") && !boost::iequals(fmt, "webp") &&
        !boost::iequals(fmt, "geojson") && !boost::iequals(fmt, "topojson") &&
        !boost::iequals(fmt, "kml") && !boost::iequals(fmt, "cnf"))
    {
      throw Fmi::Exception(BCP, "Invalid 'type' value '" + fmt + "'!");
    }

    if (fmt == "cnf")
    {
      theResponse.setHeader("Content-Type", mimeType("cnf"));
      Json::StyledWriter writer;
      theResponse.setContent(writer.write(json));
      return;
    }

    // And initialize the product specs from the JSON

    Product product;
    product.init(json, theState, itsConfig);
    if (product.type.empty())
      product.type = theState.getType();

    if (print_params)
      print(product.getGridParameterInfo(theState));

    check_remaining_dali_json(json, theState.getName());

    product.check_errors(theRequest.getURI());

    // Calculate hash for the product

    auto product_hash = product.hash_value(theState);

    if (product_hash != Fmi::bad_hash)
    {
      theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));

      // If request was ETag request, respond accordingly
      if (theRequest.getHeader("X-Request-ETag"))
      {
        theResponse.setHeader("Content-Type", mimeType(product.type));
        theResponse.setStatus(Spine::HTTP::Status::no_content);

        // Add updated expiration time if available
        const auto &expires = theState.getExpirationTime();
        if (expires)
        {
          std::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));
          theResponse.setHeader("Expires", tformat->format(*expires));
        }

        return;
      }
    }

    auto obj = itsImageCache->find(product_hash);

    if (obj)
    {
      theResponse.setHeader("Content-Type", mimeType(product.type));
      theResponse.setHeader("X-Backend-Cache", "1");
      theResponse.setContent(obj);
      return;
    }

    // Get the product template

    if (!product.svg_tmpl)
      product.svg_tmpl = itsConfig.defaultTemplate(product.type);

    auto tmpl = getTemplate(*product.svg_tmpl);

    // Build the response CDT
    CTPP::CDT hash(CTPP::CDT::HASH_VAL);
    {
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "Product::generate finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }
      product.generate(hash, theState);
    }

    if (print_hash)
    {
      std::cout << fmt::format("Generated CDT for {} {}\n{}\n",
                               theState.getCustomer(),
                               product_name,
                               hash.RecursiveDump());
    }

    std::string output;
    std::string log;
    try
    {
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "Template processing finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }
      tmpl->process(hash, output, log);
    }
    catch (const CTPP::CTPPException &e)
    {
      throw Fmi::Exception::Trace(BCP, "Template processing error!")
          .addParameter("Template", *product.svg_tmpl)
          .addParameter("Product", product_name);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Template processing error!")
          .addParameter("Template", *product.svg_tmpl)
          .addParameter("Product", product_name);
    }

    // Set the response content and mime type

    formatResponse(output, product.type, theRequest, theResponse, usetimer, product, product_hash);

    // boost auto_cpu_timer does not flush, we need to do it separately
    if (usetimer)
      std::cout << "Timed query finished\n";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Query failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert the SVG to the response object
 */
// ----------------------------------------------------------------------

void Plugin::formatResponse(const std::string &theSvg,
                            const std::string &theType,
                            const Spine::HTTP::Request &theRequest,
                            Spine::HTTP::Response &theResponse,
                            bool usetimer,
                            const Product &theProduct,
                            std::size_t theHash)
{
  try
  {
    std::set<std::string> text_formats{"xml",
                                       "svg",
                                       "image/svg+xml",
                                       "geojson",
                                       "topojson",
                                       "kml",
                                       "json",
                                       "application/json",
                                       "cnf"};

    theResponse.setHeader("Content-Type", mimeType(theType));

    if (text_formats.find(theType) != text_formats.end())
    {
      // Set string content as-is
      if (theSvg.empty())
      {
        std::cerr << fmt::format("Warning: Empty input for request {} from {}\n",
                                 theRequest.getQueryString(),
                                 theRequest.getClientIP());
      }
      else
      {
        theResponse.setContent(theSvg);
      }
    }
    else
    {
      // Convert buffer content
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (usetimer)
      {
        std::string report = "svg_to_" + theType + " finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }

      std::shared_ptr<std::string> buffer;
      if (theType == "png")
        buffer = std::make_shared<std::string>(Giza::Svg::topng(theSvg, theProduct.png.options));
      else if (theType == "webp")
        buffer = std::make_shared<std::string>(Giza::Svg::towebp(theSvg));
      else if (theType == "pdf")
        buffer = std::make_shared<std::string>(Giza::Svg::topdf(theSvg));
      else if (theType == "ps")
        buffer = std::make_shared<std::string>(Giza::Svg::tops(theSvg));
      else
        throw Fmi::Exception(BCP, "Cannot convert SVG to unknown format '" + theType + "'");

      if (theHash != Fmi::bad_hash)
      {
        itsImageCache->insert(theHash, buffer);

        // For frontend caching
        theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", theHash));
      }

      theResponse.setContent(buffer);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Response format failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main content handler
 */
// ----------------------------------------------------------------------

void Plugin::requestHandler(Spine::Reactor &theReactor,
                            const Spine::HTTP::Request &theRequest,
                            Spine::HTTP::Response &theResponse)
{
  try
  {
    const bool isdebug = Spine::optional_bool(theRequest.getParameter("debug"), false);

    theResponse.setHeader("Access-Control-Allow-Origin", "*");

    // WMS: if WMS exception is thrown or capabilities requested, the format must be xml in response
    // no matter what format-option was given in request
    try
    {
      State state(*this, theRequest);
      state.useTimer(Spine::optional_bool(theRequest.getParameter("timer"), false));

      using Fmi::DateTime;

      if (theRequest.getResource() == "/wms")
      {
        state.useWms(true);

        theResponse.setStatus(Spine::HTTP::Status::ok);

        // may modify HTTP status set above
        try
        {
          WMSQueryStatus status = wmsQuery(theReactor, state, theRequest, theResponse);

          switch (status)
          {
            case WMSQueryStatus::FORBIDDEN:
            {
              theResponse.setStatus(Spine::HTTP::Status::forbidden, true);
              break;
            }
            case WMSQueryStatus::OK:
            default:
              break;
          }
        }

        catch (const Fmi::Exception &wmsException)
        {
          // The default for most responses:
          theResponse.setStatus(Spine::HTTP::Status::bad_request);

          const Fmi::Exception *e = wmsException.getExceptionByParameterName(WMS_EXCEPTION_CODE);

          if (e != nullptr)
          {
            // Status codes used by OGC and their HTTP response codes:
            // OperationNotSupported 501 Not Implemented
            // MissingParameterValue 400 Bad request
            // InvalidParameterValue 400 Bad request
            // VersionNegotiationFailed 400 Bad request
            // InvalidUpdateSequence 400 Bad request
            // OptionNotSupported 501 Not Implemented
            // NoApplicableCode 3xx, 4xx, 5xx Internal Server Error

            std::string exceptionCode = e->getParameterValue(WMS_EXCEPTION_CODE);

            theResponse.setHeader("X-WMS-Exception", exceptionCode);

            std::string firstMessage = wmsException.what();
            boost::algorithm::replace_all(firstMessage, "\n", " ");
            if (firstMessage.size() > 300)
              firstMessage.resize(300);
            theResponse.setHeader("X-WMS-Error", firstMessage);

            if (exceptionCode == WMS_LAYER_NOT_QUERYABLE ||
                exceptionCode == WMS_OPERATION_NOT_SUPPORTED)
            {
              theResponse.setStatus(Spine::HTTP::Status::not_implemented);
            }
          }
        }
      }
      else
      {
        state.useWms(false);

        theResponse.setStatus(Spine::HTTP::Status::ok);
        daliQuery(theReactor, state, theRequest, theResponse);
      }

      // Adding headers

      std::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));

      const Fmi::DateTime t_now = Fmi::SecondClock::universal_time();
      const auto &modification_time = state.getModificationTime();
      if (!modification_time)
        theResponse.setHeader("Last-Modified", tformat->format(t_now));
      else
        theResponse.setHeader("Last-Modified", tformat->format(*modification_time));

      // Send expiration header only if there was no error. Note: 304 Not Modified must pass!

      bool is_error = (static_cast<int>(theResponse.getStatus()) >= 400);

      if (!is_error)
      {
        const auto &expires = state.getExpirationTime();
        if (!expires)
          theResponse.setHeader("Expires", tformat->format(t_now + Fmi::Hours(1)));
        else
          theResponse.setHeader("Expires", tformat->format(*expires));
      }
    }
    catch (...)
    {
      // Catching all exceptions

      Fmi::Exception exception(BCP, "Request processing exception!", nullptr);
      exception.addParameter("URI", theRequest.getURI());
      exception.addParameter("ClientIP", theRequest.getClientIP());
      exception.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));

      const bool check_token = true;
      auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
      exception.addParameter("Apikey", (apikey ? *apikey : std::string("-")));

      auto quiet = theRequest.getParameter("quiet");
      if (!quiet || (*quiet == "0" && *quiet == "false"))
        exception.printError();

      if (isdebug)
      {
        // Delivering the exception information as HTTP content
        std::string fullMessage = exception.getHtmlStackTrace();
        theResponse.setContent(fullMessage);
        theResponse.setStatus(Spine::HTTP::Status::ok);
      }
      else
      {
        theResponse.setStatus(Spine::HTTP::Status::bad_request);
      }

      // Adding the first exception information into the response header

      std::string firstMessage = exception.what();
      boost::algorithm::replace_all(firstMessage, "\n", " ");
      if (firstMessage.size() > 300)
        firstMessage.resize(300);
      theResponse.setHeader("X-Dali-Error", firstMessage);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Plugin constructor
 */
// ----------------------------------------------------------------------

Plugin::Plugin(Spine::Reactor *theReactor, const char *theConfig)
    : itsModuleName("WMS"),
      itsConfig(theConfig),
      itsReactor(theReactor),
      itsGridEngine(nullptr),
      itsQEngine(nullptr),
      itsContourEngine(nullptr),
      itsGisEngine(nullptr),
#ifdef WITHOUT_OBSERVATION
      itsGeoEngine(nullptr)
#else
      itsGeoEngine(nullptr),
      itsObsEngine(nullptr)
#endif

{
  try
  {
    if (theReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
    {
      std::cerr << fmt::format(
          "{}{} * **Dali Plugin and Server SmartMet API version mismatch ** *{} {}\n ",
          ANSI_BOLD_ON,
          ANSI_FG_RED,
          ANSI_FG_DEFAULT,
          ANSI_BOLD_OFF);
      return;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize in a separate thread due to PostGIS slowness
 */
// ----------------------------------------------------------------------

void Plugin::init()
{
  try
  {
    // Imagecache

    itsImageCache = std::make_unique<ImageCache>(itsConfig.maxMemoryCacheSize(),
                                                 itsConfig.maxFilesystemCacheSize(),
                                                 itsConfig.filesystemCacheDirectory());

    // StyleSheet cache
    itsStyleSheetCache.resize(itsConfig.styleSheetCacheSize());

    // CONTOUR

    if (Spine::Reactor::isShuttingDown())
      return;

    auto *engine = itsReactor->getSingleton("Contour", nullptr);
    if (engine == nullptr)
      throw Fmi::Exception(BCP, "Contour engine unavailable");
    itsContourEngine = reinterpret_cast<Engine::Contour::Engine *>(engine);
    if (Spine::Reactor::isShuttingDown())
      return;

    // GIS

    if (Spine::Reactor::isShuttingDown())
      return;

    engine = itsReactor->getSingleton("Gis", nullptr);
    if (engine == nullptr)
      throw Fmi::Exception(BCP, "Gis engine unavailable");
    itsGisEngine = reinterpret_cast<Engine::Gis::Engine *>(engine);
    if (Spine::Reactor::isShuttingDown())
      return;

    // QUERYDATA

    if (Spine::Reactor::isShuttingDown())
      return;

    engine = itsReactor->getSingleton("Querydata", nullptr);
    if (engine == nullptr)
      throw Fmi::Exception(BCP, "Querydata engine unavailable");
    itsQEngine = reinterpret_cast<Engine::Querydata::Engine *>(engine);
    if (Spine::Reactor::isShuttingDown())
      return;

    // GEONAMES

    engine = itsReactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
      throw Fmi::Exception(BCP, "Geonames engine unavailable");
    itsGeoEngine = reinterpret_cast<Engine::Geonames::Engine *>(engine);
    if (Spine::Reactor::isShuttingDown())
      return;

    // GRID ENGINE
    if (!itsConfig.gridEngineDisabled())
    {
      engine = itsReactor->getSingleton("grid", nullptr);
      if (engine == nullptr)
        throw Fmi::Exception(BCP, "Grid engine unavailable");
      itsGridEngine = reinterpret_cast<Engine::Grid::Engine *>(engine);
      itsGridEngine->setDem(itsGeoEngine->dem());
      itsGridEngine->setLandCover(itsGeoEngine->landCover());
    }

    // OBSERVATION

    if (Spine::Reactor::isShuttingDown())
      return;

#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      engine = itsReactor->getSingleton("Observation", nullptr);
      if (engine == nullptr)
        throw Fmi::Exception(BCP, "Observation engine unavailable");
      itsObsEngine = reinterpret_cast<Engine::Observation::Engine *>(engine);
    }
#endif

    if (Spine::Reactor::isShuttingDown())
      return;

#ifndef WITHOUT_AUTHENTICATION

    // AUTHENTICATION
    if (itsConfig.authenticate())
    {
      engine = itsReactor->getSingleton("Authentication", nullptr);
      if (engine == nullptr)
        throw Fmi::Exception(BCP, "Authentication unavailable");
      auto *authEngine = reinterpret_cast<Engine::Authentication::Engine *>(engine);

// WMS configurations
#ifndef WITHOUT_OBSERVATION
      itsWMSConfig = std::make_unique<WMS::WMSConfig>(itsConfig,
                                                      itsJsonCache,
                                                      itsQEngine,
                                                      authEngine,
                                                      itsObsEngine,
                                                      itsGisEngine,
                                                      itsGridEngine);
#else
      itsWMSConfig = std::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, authEngine, itsGisEngine);
#endif
    }
    else
    {
#ifndef WITHOUT_OBSERVATION
      itsWMSConfig = std::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, nullptr, itsObsEngine, itsGisEngine, itsGridEngine);
#else
      itsWMSConfig = std::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, nullptr, itsGisEngine);
#endif
    }

#else
#ifndef WITHOUT_OBSERVATION
    itsWMSConfig = std::make_unique<WMS::WMSConfig>(
        itsConfig, itsJsonCache, itsQEngine, itsObsEngine, itsGisEngine, itsGridEngine);
#else
    itsWMSConfig = std::make_unique<WMS::WMSConfig>(
        itsConfig, itsJsonCache, itsQEngine, itsGisEngine, itsGridEngine);
#endif
#endif

    if (Spine::Reactor::isShuttingDown())
      itsWMSConfig->shutdown();

    itsWMSConfig->init();  // heavy initializations

    if (Spine::Reactor::isShuttingDown())
      itsWMSConfig->shutdown();

    // Register dali content handler

    if (!itsReactor->addContentHandler(this,
                                       itsConfig.defaultUrl(),
                                       [this](Spine::Reactor &theReactor,
                                              const Spine::HTTP::Request &theRequest,
                                              Spine::HTTP::Response &theResponse) {
                                         callRequestHandler(theReactor, theRequest, theResponse);
                                       }))

      throw Fmi::Exception(BCP, "Failed to register Dali content handler");

    // Register WMS content handler

    if (!itsReactor->addContentHandler(this,
                                       itsConfig.wmsUrl(),
                                       [this](Spine::Reactor &theReactor,
                                              const Spine::HTTP::Request &theRequest,
                                              Spine::HTTP::Response &theResponse) {
                                         callRequestHandler(theReactor, theRequest, theResponse);
                                       }))
      throw Fmi::Exception(BCP, "Failed to register WMS content handler");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Init failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the plugin
 */
// ----------------------------------------------------------------------

void Plugin::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (dali)\n" << std::flush;

    if (itsImageCache != nullptr)
      itsImageCache->shutdown();

    if (itsWMSConfig != nullptr)
      itsWMSConfig->shutdown();  // will wait for threads to finish
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Server thread pool determination
 */
// ----------------------------------------------------------------------

bool Plugin::queryIsFast(const Spine::HTTP::Request &theRequest) const
{
  try
  {
    // WMS requests should be handled ASAP, others, not so much
    return (theRequest.getResource() == "/wms");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin()
{
  if (itsImageCache != nullptr)
    itsImageCache->shutdown();

  if (itsWMSConfig != nullptr)
    itsWMSConfig->shutdown();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the plugin name
 */
// ----------------------------------------------------------------------

const std::string &Plugin::getPluginName() const
{
  return itsModuleName;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the required version
 */
// ----------------------------------------------------------------------

int Plugin::getRequiredAPIVersion() const
{
  return SMARTMET_API_VERSION;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the product JSON
 */
// ----------------------------------------------------------------------

Json::Value Plugin::getProductJson(const Spine::HTTP::Request &theRequest,
                                   const State &theState,
                                   const std::string &theName,
                                   int stage) const
{
  try
  {
    // Establish the path to the JSON file.

    std::string customer_root =
        (itsConfig.rootDirectory(theState.useWms()) + "/customers/" + theState.getCustomer());

    std::string product_path = customer_root + "/products/" + theName + ".json";

    if (!std::filesystem::exists(product_path))
    {
      throw Fmi::Exception(BCP, "Product file not found!").addParameter("File", product_path);
    }

    // Read the JSON

    std::string json_text = itsFileCache.get(product_path);

    Json::Value json;
    std::unique_ptr<Json::CharReader> reader(charreaderbuilder.newCharReader());
    std::string errors;
    if (!reader->parse(json_text.c_str(), json_text.c_str() + json_text.size(), &json, &errors))
      throw Fmi::Exception(BCP, "Legend template file parsing failed!")
          .addParameter("Product", product_path)
          .addParameter("Message", errors);

    if (stage == 1)
      return json;

    // Replace references (json: and ref:) from query string options

    auto params = extractValidParameters(theRequest.getParameterMap());

    Spine::JSON::replaceReferences(json, params);

    if (stage == 2)
      return json;

    // Expand the JSON

    std::string layers_root = customer_root + "/layers/";

    Spine::JSON::preprocess(
        json, itsConfig.rootDirectory(theState.useWms()), layers_root, itsJsonCache);

    if (stage == 3)
      return json;

    // Expand paths

    Spine::JSON::dereference(json);

    if (stage == 4)
      return json;

    // Modify variables as requested (not reference substitutions)

    Spine::JSON::expand(json, params, "", false);

    return json;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string Plugin::resolveFilePath(const std::string &theCustomer,
                                    const std::string &theSubDir,
                                    const std::string &theFileName,
                                    bool theWmsFlag,
                                    std::list<std::string> &theTestedPaths) const
{
  if (theCustomer.empty() || theFileName.empty())
    return "";

  try
  {
    std::string file_path;
    std::string filename = theFileName;

    // clang-format off
    /*
     * 1) If file name starts with '/' search from wms-root
     * 2) else check existence of file first from customer-path then from wms-root
     * 3) In wms-root check in the following order
     * 3.1) wms-root
     * 3.2) wms-root/resources/layers/<theSubDir> except for filters check wms-root/resources/filters
     * 3.3) wms-root/resources/<theSubDir>
     * 3.4) wms-root/resources
     */
    // clang-format on

    if (filename[0] != '/')
    {
      file_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + check_attack(theCustomer) +
                   theSubDir + check_attack(theFileName));
      theTestedPaths.push_back(file_path);
      if (!std::filesystem::exists(file_path))
        filename.insert(filename.begin(), '/');
    }
    if (filename[0] == '/')
    {
      file_path = itsConfig.rootDirectory(theWmsFlag) + check_attack(filename);
      theTestedPaths.push_back(file_path);
      if (!std::filesystem::exists(file_path))
      {
        if (theSubDir == "/filters/")
        {
          file_path =
              itsConfig.rootDirectory(theWmsFlag) + "/resources/filters" + check_attack(filename);
          theTestedPaths.push_back(file_path);
        }
        else
        {
          file_path = itsConfig.rootDirectory(theWmsFlag) + "/resources/layers" + theSubDir +
                      check_attack(filename);
          theTestedPaths.push_back(file_path);
          if (!std::filesystem::exists(file_path))
            file_path = itsConfig.rootDirectory(theWmsFlag) + "/resources" + theSubDir +
                        check_attack(filename);
          theTestedPaths.push_back(file_path);
          if (!std::filesystem::exists(file_path))
            file_path = itsConfig.rootDirectory(theWmsFlag) + "/resources" + check_attack(filename);
          theTestedPaths.push_back(file_path);
        }
      }
    }

    return file_path;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving file name failed: " + theFileName);
  }
}

std::string Plugin::resolveSvgPath(const std::string &theCustomer,
                                   const std::string &theSubDir,
                                   const std::string &theFileName,
                                   bool theWmsFlag) const
{
  if (theCustomer.empty() || theFileName.empty())
    return {};

  std::list<std::string> tested_paths;

  auto file =
      resolveFilePath(theCustomer, theSubDir, theFileName + ".svg", theWmsFlag, tested_paths);
  if (std::filesystem::exists(file))
    return file;

  file = resolveFilePath(theCustomer, theSubDir, theFileName, theWmsFlag, tested_paths);
  if (std::filesystem::exists(file))
    return file;

  return file;

#if 0
  // Occasionally useful for debugging. We cannot require the file to exist though,
  // a "filter(#url)" may be an internal link instead of a reference to an external file
  
  throw Fmi::Exception(BCP, "File does not exist")
      .addParameter("Customer", theCustomer)
      .addParameter("Subdirectory", theSubDir)
      .addParameter("Name", theFileName)
      .addDetails(tested_paths);
#endif
}

// ----------------------------------------------------------------------
/*!
 * \brief Get CSS contents from the internal cache
 */
// ----------------------------------------------------------------------

std::string Plugin::getStyle(const std::string &theCustomer,
                             const std::string &theCSS,
                             bool theWmsFlag) const
{
  try
  {
    if (theCustomer.empty() || theCSS.empty())
      return "";

    std::list<std::string> tested_files;
    std::string css_path =
        resolveFilePath(theCustomer, "/layers/", theCSS, theWmsFlag, tested_files);

    if (std::filesystem::exists(css_path))
      return itsFileCache.get(css_path);

    throw Fmi::Exception(BCP, "Failed to find CSS file").addDetails(tested_files);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find style")
        .addParameter("Customer", theCustomer)
        .addParameter("CSS", theCSS);
    ;
  }
}

std::map<std::string, std::string> Plugin::getStyle(const std::string &theCustomer,
                                                    const std::string &theCSS,
                                                    bool theWmsFlag,
                                                    const std::string &theSelector)
{
  try
  {
    auto css = getStyle(theCustomer, theCSS, theWmsFlag);

    if (!css.empty())
    {
      const auto hash = Fmi::hash_value(theCSS);
      auto style = itsStyleSheetCache.find(hash);
      if (style)
        return style->declarations(theSelector);

      StyleSheet ss;
      ss.add(css);
      itsStyleSheetCache.insert(hash, ss);
      return ss.declarations(theSelector);
    }

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find style")
        .addParameter("Customer", theCustomer)
        .addParameter("CSS", theCSS)
        .addParameter("Selector", theSelector);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get template from the plugin cache
 */
// ----------------------------------------------------------------------

Fmi::SharedFormatter Plugin::getTemplate(const std::string &theName) const
{
  try
  {
    std::string tmpl_path = (itsConfig.templateDirectory() + "/" + check_attack(theName) + ".c2t");

    return itsTemplateFactory.get(tmpl_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find template").addParameter("Template", theName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Filter contents from the internal cache
 */
// ----------------------------------------------------------------------

std::string Plugin::getFilter(const std::string &theCustomer,
                              const std::string &theName,
                              bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return "";

    std::string filter_path = resolveSvgPath(theCustomer, "/filters/", theName, theWmsFlag);

    return itsFileCache.get(filter_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find filter")
        .addParameter("Customer", theCustomer)
        .addParameter("Filter", theName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Filter hash
 */
// ----------------------------------------------------------------------

std::size_t Plugin::getFilterHash(const std::string &theCustomer,
                                  const std::string &theName,
                                  bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return 0;

    std::string filter_path = resolveSvgPath(theCustomer, "/filters/", theName, theWmsFlag);

    return itsFileCache.last_modified(filter_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get marker contents from the internal cache
 */
// ----------------------------------------------------------------------

std::string Plugin::getMarker(const std::string &theCustomer,
                              const std::string &theName,
                              bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return "";

    std::string marker_path = resolveSvgPath(theCustomer, "/markers/", theName, theWmsFlag);

    return itsFileCache.get(marker_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find marker")
        .addParameter("Customer", theCustomer)
        .addParameter("Marker", theName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get marker hash
 */
// ----------------------------------------------------------------------

std::size_t Plugin::getMarkerHash(const std::string &theCustomer,
                                  const std::string &theName,
                                  bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return 0;

    std::string marker_path = resolveSvgPath(theCustomer, "/markers/", theName, theWmsFlag);

    return itsFileCache.last_modified(marker_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find marker hash")
        .addParameter("Customer", theCustomer)
        .addParameter("Marker", theName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Symbol contents from the internal cache
 */
// ----------------------------------------------------------------------

std::string Plugin::getSymbol(const std::string &theCustomer,
                              const std::string &theName,
                              bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return "";

    std::string symbol_path = resolveSvgPath(theCustomer, "/symbols/", theName, theWmsFlag);

    return itsFileCache.get(symbol_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find symbol!")
        .addParameter("Customer", theCustomer)
        .addParameter("Symbol", theName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Symbol hash
 */
// ----------------------------------------------------------------------

std::size_t Plugin::getSymbolHash(const std::string &theCustomer,
                                  const std::string &theName,
                                  bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return 0;

    std::string symbol_path = resolveSvgPath(theCustomer, "/symbols/", theName, theWmsFlag);

    return itsFileCache.last_modified(symbol_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Pattern contents from the internal cache
 */
// ----------------------------------------------------------------------

std::string Plugin::getPattern(const std::string &theCustomer,
                               const std::string &theName,
                               bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return "";

    std::string pattern_path = resolveSvgPath(theCustomer, "/patterns/", theName, theWmsFlag);

    return itsFileCache.get(pattern_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find pattern")
        .addParameter("Customer", theCustomer)
        .addParameter("Pattern", theName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Pattern hash
 */
// ----------------------------------------------------------------------

std::size_t Plugin::getPatternHash(const std::string &theCustomer,
                                   const std::string &theName,
                                   bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return 0;

    std::string pattern_path = resolveSvgPath(theCustomer, "/patterns/", theName, theWmsFlag);

    return itsFileCache.last_modified(pattern_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Gradient contents from the internal cache
 */
// ----------------------------------------------------------------------

std::string Plugin::getGradient(const std::string &theCustomer,
                                const std::string &theName,
                                bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return "";

    std::string gradient_path = resolveSvgPath(theCustomer, "/gradients/", theName, theWmsFlag);

    return itsFileCache.get(gradient_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to find gradient")
        .addParameter("Customer", theCustomer)
        .addParameter("Gradient", theName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Gradient hash
 */
// ----------------------------------------------------------------------

std::size_t Plugin::getGradientHash(const std::string &theCustomer,
                                    const std::string &theName,
                                    bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return 0;

    std::string gradient_path = resolveSvgPath(theCustomer, "/gradients/", theName, theWmsFlag);

    return itsFileCache.last_modified(gradient_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the configuration object - layers need to know defaults
 */
// ----------------------------------------------------------------------

const Config &Plugin::getConfig() const
{
  return itsConfig;
}

Fmi::Cache::CacheStatistics Plugin::getCacheStats() const
{
  Fmi::Cache::CacheStatistics ret;

  ret["Wms::image_cache::memory_cache [B]"] = itsImageCache->getMemoryCacheStats();
  ret["Wms::image_cache::file_cache [B]"] = itsImageCache->getFileCacheStats();
  ret["Wms::css_cache"] = itsStyleSheetCache.statistics();

  return ret;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

/*
 * Server knows us through the 'SmartMetPlugin' virtual interface, which
 * the 'Plugin' class implements.
 */

extern "C" SmartMetPlugin *create(SmartMet::Spine::Reactor *them, const char *config)
{
  return new SmartMet::Plugin::Dali::Plugin(them, config);
}

extern "C" void destroy(SmartMetPlugin *us)
{
  // This will call 'Plugin::~Plugin()' since the destructor is virtual
  delete us;  // NOLINT(cppcoreguidelines-owning-memory)
}
