// ======================================================================
/*!
 * \brief SmartMet Dali plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "Product.h"
#include "State.h"
#include "WMSConfig.h"
#include "WMSGetLegendGraphic.h"
#include "WMSGetMap.h"
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/Json.h>
#include <spine/SmartMet.h>
#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/move/make_unique.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/timer/timer.hpp>
#include <boost/utility.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <giza/Svg.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <stdexcept>

namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Validate name for inclusion
 *
 * A valid name does not lead upwards in a path if inserted into
 * a path. If the name looks valid, we return it so that we do not
 * have to use if statements when this check is done. If the name
 * does not look valid, we throw.
 */
// ----------------------------------------------------------------------

const std::string &check_attack(const std::string &theName)
{
  try
  {
    if (theName.find("./") == std::string::npos)
      return theName;

    throw SmartMet::Spine::Exception(
        BCP, "Attack IRI detected, relative paths upwards are not safe: '" + theName + "'");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
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
    int width = Spine::optional_int(theRequest.getParameter("width"), 1000);
    if (width < 20 || width > 10000)
    {
      throw Spine::Exception(BCP, "Invalid 'width' value '" + std::to_string(width) + "'!");
    }

    int height = Spine::optional_int(theRequest.getParameter("height"), 1000);
    if (height < 20 || height > 10000)
    {
      throw Spine::Exception(BCP, "Invalid 'height' value '" + std::to_string(height) + "'!");
    }

    // Establish debugging related variables

    const bool print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);

    const bool print_json = Spine::optional_bool(theRequest.getParameter("printjson"), false);

    const bool usetimer = Spine::optional_bool(theRequest.getParameter("timer"), false);

    // Define the customer and image format

    theState.setCustomer(
        Spine::optional_string(theRequest.getParameter("customer"), itsConfig.defaultCustomer()));

    if (theState.getCustomer().empty())
      throw Spine::Exception(BCP, "Customer setting is empty");

    // Get the product configuration

    std::string product_name = Spine::required_string(
        theRequest.getParameter("product"), "Product configuration option 'product' not given");
    auto product = getProduct(theRequest, theState, product_name, print_json);

    // Trivial error checks done first for speed

    if (product.type.empty())
      product.type = "svg";

    if (!boost::iequals(product.type, "xml") && !boost::iequals(product.type, "svg") &&
        !boost::iequals(product.type, "png") && !boost::iequals(product.type, "pdf") &&
        !boost::iequals(product.type, "ps") && !boost::iequals(product.type, "geojson") &&
        !boost::iequals(product.type, "kml"))

    {
      throw Spine::Exception(BCP, "Invalid 'type' value '" + product.type + "'!");
    }

    // Image format can no longer be changed by anything, provide the info to layers
    theState.setType(product.type);

    // Calculate hash for the product

    auto product_hash = product.hash_value(theState);

    // If request was ETag request, respond accordingly
    if (theRequest.getHeader("X-Request-ETag"))
    {
      theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));
      theResponse.setHeader("Content-Type", mimeType(product.type));
      theResponse.setStatus(Spine::HTTP::Status::no_content);

      // Add updated expiration time if available
      const auto &expires = theState.getExpirationTime();
      if (expires)
      {
        boost::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));
        theResponse.setHeader("Expires", tformat->format(*expires));
      }
      return;
    }

    auto obj = itsImageCache->find(product_hash);

#ifdef MYDEBUG
    std::cout << "Product hash_value = " << product_hash << std::endl;
#endif

    if (obj)
    {
#ifdef MYDEBUG
      std::cout << "\treturning cached image" << std::endl;
#endif
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
      std::string report = "Product::generate finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
      product.generate(hash, theState);
    }

    if (print_hash)
    {
      std::cout << "Generated CDT for " << theState.getCustomer() << " " << product_name
                << std::endl
                << hash.RecursiveDump() << std::endl;
    }

    std::ostringstream output, log;
    try
    {
      std::string report = "Template processing finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
      tmpl->process(hash, output, log);
    }
    catch (const CTPP::CTPPException &e)
    {
      throw Spine::Exception::Trace(BCP, "Template processing error!")
          .addParameter("Template", *product.svg_tmpl)
          .addParameter("Product", product_name);
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Template processing error!")
          .addParameter("Template", *product.svg_tmpl)
          .addParameter("Product", product_name);
    }

    // Set the response content and mime type

    formatResponse(
        output.str(), product.type, theRequest, theResponse, usetimer, product, product_hash);

    // boost auto_cpu_timer does not flush, we need to do it separately
    if (usetimer)
      std::cout << "Timed query finished" << std::endl;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Query failed!");
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
    theResponse.setHeader("Content-Type", mimeType(theType));

    if (theType == "xml" || theType == "svg" || theType == "image/svg+xml" ||
        theType == "geojson" || theType == "kml")
    {
      // Set string content as-is
      if (theSvg.empty())
      {
        std::cerr << "Warning: Empty input for request " << theRequest.getQueryString() << " from "
                  << theRequest.getClientIP() << std::endl;
      }
      else
      {
        theResponse.setContent(theSvg);
      }
    }
    else
    {
      // Convert buffer content
      std::string report = "svg_to_" + theType + " finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (usetimer)
        mytimer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

      boost::shared_ptr<std::string> buffer;
      if (theType == "png")
        buffer = boost::make_shared<std::string>(Giza::Svg::topng(theSvg, theProduct.png.options));
      else if (theType == "pdf")
        buffer = boost::make_shared<std::string>(Giza::Svg::topdf(theSvg));
      else if (theType == "ps")
        buffer = boost::make_shared<std::string>(Giza::Svg::tops(theSvg));
      else
        throw Spine::Exception(BCP, "Cannot convert SVG to unknown format '" + theType + "'");

      if (theHash != 0)
      {
#ifdef MYDEBUG
        std::cout << "Inserting product to cache with hash " << theHash << std::endl;
#endif
        itsImageCache->insert(theHash, buffer);

        // For frontend caching
        theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", theHash));
      }

      theResponse.setContent(buffer);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Response format failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the mime type for the product
 */
// ----------------------------------------------------------------------

std::string Plugin::mimeType(const std::string &theType) const
{
  if (theType == "xml")
    return "text/xml; charset=UTF-8";
  if (theType == "svg")
    return "image/svg+xml; charset=UTF-8";
  if (theType == "png")
    return "image/png";
  if (theType == "pdf")
    return "application/pdf";
  if (theType == "ps")
    return "application/postscript";
  if (theType == "geojson")
    return "application/geo+json";
  if (theType == "kml")
    return "application/vnd.google-earth.kml+xml";

  throw Spine::Exception(BCP, "Unknown image format '" + theType + "'");
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
      State state(*this);
      state.useTimer(Spine::optional_bool(theRequest.getParameter("timer"), false));

      using boost::posix_time::ptime;

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

        catch (const Spine::Exception &wmsException)
        {
          // The default for most responses:
          theResponse.setStatus(Spine::HTTP::Status::bad_request);

          const Spine::Exception *e = wmsException.getExceptionByParameterName(WMS_EXCEPTION_CODE);
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
            firstMessage = firstMessage.substr(0, 300);
            theResponse.setHeader("X-WMS-Error", firstMessage.c_str());

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

      boost::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));

      const ptime t_now = boost::posix_time::second_clock::universal_time();
      theResponse.setHeader("Last-Modified", tformat->format(t_now));

      // Send expiration header only if there was no error
      bool is_error = (static_cast<int>(theResponse.getStatus()) < 300);
      if (!is_error)
      {
        const auto &expires = state.getExpirationTime();
        if (!expires)
          theResponse.setHeader("Expires", tformat->format(t_now + boost::posix_time::hours(1)));
        else
          theResponse.setHeader("Expires", tformat->format(*expires));
      }
    }
    catch (...)
    {
      // Catching all exceptions

      Spine::Exception exception(BCP, "Request processing exception!", nullptr);
      exception.addParameter("URI", theRequest.getURI());
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
      firstMessage = firstMessage.substr(0, 300);
      theResponse.setHeader("X-Dali-Error", firstMessage.c_str());
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      itsQEngine(nullptr),
      itsContourEngine(nullptr),
      itsGisEngine(nullptr),
      itsGeoEngine(nullptr),
#ifndef WITHOUT_OBSERVATION
      itsObsEngine(nullptr),
#endif
      itsWMSGetCapabilities()
{
  try
  {
    if (theReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
    {
      std::cerr << ANSI_BOLD_ON << ANSI_FG_RED
                << "*** Dali Plugin and Server SmartMet API version mismatch ***" << ANSI_FG_DEFAULT
                << ANSI_BOLD_OFF << std::endl;
      return;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    itsImageCache = boost::movelib::make_unique<ImageCache>(itsConfig.maxMemoryCacheSize(),
                                                            itsConfig.maxFilesystemCacheSize(),
                                                            itsConfig.filesystemCacheDirectory());

    // CONTOUR

    if (itsShutdownRequested)
      return;

    auto engine = itsReactor->getSingleton("Contour", nullptr);
    if (engine == nullptr)
      throw Spine::Exception(BCP, "Contour engine unavailable");
    itsContourEngine = reinterpret_cast<Engine::Contour::Engine *>(engine);
    if (itsShutdownRequested)
      return;

    // GIS

    if (itsShutdownRequested)
      return;

    engine = itsReactor->getSingleton("Gis", nullptr);
    if (engine == nullptr)
      throw Spine::Exception(BCP, "Gis engine unavailable");
    itsGisEngine = reinterpret_cast<Engine::Gis::Engine *>(engine);
    if (itsShutdownRequested)
      return;

    // QUERYDATA

    if (itsShutdownRequested)
      return;

    engine = itsReactor->getSingleton("Querydata", nullptr);
    if (engine == nullptr)
      throw Spine::Exception(BCP, "Querydata engine unavailable");
    itsQEngine = reinterpret_cast<Engine::Querydata::Engine *>(engine);
    if (itsShutdownRequested)
      return;

    // GEONAMES

    engine = itsReactor->getSingleton("Geonames", nullptr);
    if (engine == nullptr)
      throw Spine::Exception(BCP, "Geonames engine unavailable");
    itsGeoEngine = reinterpret_cast<Engine::Geonames::Engine *>(engine);
    if (itsShutdownRequested)
      return;

    // OBSERVATION

    if (itsShutdownRequested)
      return;

#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      engine = itsReactor->getSingleton("Observation", nullptr);
      if (!engine)
        throw Spine::Exception(BCP, "Observation engine unavailable");
      itsObsEngine = reinterpret_cast<Engine::Observation::Engine *>(engine);
    }
#endif

    if (itsShutdownRequested)
      return;

#ifndef WITHOUT_AUTHENTICATION

    // AUTHENTICATION
    if (itsConfig.authenticate())
    {
      engine = itsReactor->getSingleton("Authentication", nullptr);
      if (engine == nullptr)
        throw Spine::Exception(BCP, "Authentication unavailable");
      auto *authEngine = reinterpret_cast<Engine::Authentication::Engine *>(engine);

// WMS configurations
#ifndef WITHOUT_OBSERVATION
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, authEngine, itsObsEngine, itsGisEngine);
#else
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, authEngine, itsGisEngine);
#endif
    }
    else
    {
#ifndef WITHOUT_OBSERVATION
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, nullptr, itsObsEngine, itsGisEngine);
#else
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, nullptr, itsGisEngine);
#endif
    }

#else
#ifndef WITHOUT_OBSERVATION
    itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
        itsConfig, itsJsonCache, itsQEngine, itsObsEngine, itsGisEngine);
#else
    itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
        itsConfig, itsJsonCache, itsQEngine, itsGisEngine);
#endif
#endif

    if (itsShutdownRequested)
      itsWMSConfig->shutdown();

    itsWMSConfig->init();  // heavy initializations

    if (itsShutdownRequested)
      itsWMSConfig->shutdown();

    itsWMSGetCapabilities = boost::movelib::make_unique<WMS::WMSGetCapabilities>(
        itsConfig.templateDirectory() + "/wms_get_capabilities.c2t");

    // Register dali content handler

    if (!itsReactor->addContentHandler(this,
                                       itsConfig.defaultUrl(),
                                       boost::bind(&Plugin::callRequestHandler, this, _1, _2, _3)))
      throw Spine::Exception(BCP, "Failed to register Dali content handler");

    // Register WMS content handler

    if (!itsReactor->addContentHandler(
            this, itsConfig.wmsUrl(), boost::bind(&Plugin::callRequestHandler, this, _1, _2, _3)))
      throw Spine::Exception(BCP, "Failed to register WMS content handler");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Init failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
 * \brief Get product definition from the plugin cache
 */
// ----------------------------------------------------------------------

Product Plugin::getProduct(const Spine::HTTP::Request &theRequest,
                           const State &theState,
                           const std::string &theName,
                           bool print_json) const
{
  try
  {
    // Establish the path to the JSON file.

    std::string customer_root =
        (itsConfig.rootDirectory(theState.useWms()) + "/customers/" + theState.getCustomer());

    std::string product_path = customer_root + "/products/" + theName + ".json";

    if (!boost::filesystem::exists(product_path))
    {
      throw Spine::Exception(BCP, "Product file not found!").addParameter("File", product_path);
    }

    // Read the JSON

    Json::Value json;
    Json::Reader reader;
    std::string json_text = itsFileCache.get(product_path);
    bool json_ok = reader.parse(json_text, json);

    if (!json_ok)
    {
      std::string msg = reader.getFormattedErrorMessages();
      std::replace(msg.begin(), msg.end(), '\n', ' ');
      throw Spine::Exception(BCP, "Product parsing failed!")
          .addDetail(msg)
          .addParameter("Product", product_path);
    }

    // Replace references (json: and ref:) from query string options

    Spine::JSON::replaceReferences(json, theRequest.getParameterMap());

    // Expand the JSON

    std::string layers_root = customer_root + "/layers/";

    Spine::JSON::preprocess(
        json, itsConfig.rootDirectory(theState.useWms()), layers_root, itsJsonCache);

    // Expand paths

    Spine::JSON::dereference(json);

    // Modify variables as requested (not reference substitutions)

    Spine::JSON::expand(json, theRequest.getParameterMap(), "", false);

    // Debugging

    if (print_json)
    {
      Json::StyledWriter writer;
      std::cout << "Expanded " << theName << " Spine::JSON:" << std::endl
                << writer.write(json) << std::endl;
    }

    // And initialize the product specs from the JSON

    Product product;
    product.init(json, theState, itsConfig);

    return product;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
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

    std::string css_path;
    if (theCSS[0] != '/')
    {
      css_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + check_attack(theCustomer) +
                  "/layers/" + check_attack(theCSS));
    }
    else
    {
      css_path = itsConfig.rootDirectory(theWmsFlag) + check_attack(theCSS);
    }

    return itsFileCache.get(css_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get template from the plugin cache
 */
// ----------------------------------------------------------------------

SharedFormatter Plugin::getTemplate(const std::string &theName) const
{
  try
  {
    std::string tmpl_path = (itsConfig.templateDirectory() + "/" + check_attack(theName) + ".c2t");

    return itsTemplateFactory.get(tmpl_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Filter contents from the internal cache
 */
// ----------------------------------------------------------------------

std::string Plugin::getFilter(const std::string &theName, bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return "";

    std::string filter_path =
        (itsConfig.rootDirectory(theWmsFlag) + "/resources/filters/" + check_attack(theName));

    return itsFileCache.get(filter_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Filter hash
 */
// ----------------------------------------------------------------------

std::size_t Plugin::getFilterHash(const std::string &theName, bool theWmsFlag) const
{
  try
  {
    if (theName.empty())
      return 0;

    std::string filter_path =
        (itsConfig.rootDirectory(theWmsFlag) + "/resources/filters/" + check_attack(theName));

    return itsFileCache.last_modified(filter_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string marker_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                               "/markers/" + check_attack(theName));

    return itsFileCache.get(marker_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string marker_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                               "/markers/" + check_attack(theName));

    return itsFileCache.last_modified(marker_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string symbol_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                               "/symbols/" + check_attack(theName));

    return itsFileCache.get(symbol_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string symbol_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                               "/symbols/" + check_attack(theName));

    return itsFileCache.last_modified(symbol_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string pattern_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                                "/patterns/" + check_attack(theName));

    return itsFileCache.get(pattern_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string pattern_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                                "/patterns/" + check_attack(theName));

    return itsFileCache.last_modified(pattern_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string gradient_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                                 "/gradients/" + check_attack(theName));

    return itsFileCache.get(gradient_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    std::string gradient_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + theCustomer +
                                 "/gradients/" + check_attack(theName));

    return itsFileCache.last_modified(gradient_path);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
// ----------------------------------------------------------------------
/*!
 * \brief Parses WMS exception
 */
// ----------------------------------------------------------------------

std::string Dali::Plugin::parseWMSException(Spine::Exception &wmsException, bool isdebug) const
{
  try
  {
    // std::cerr << wmsException;

    CTPP::CDT hash;

    const Spine::Exception *e = wmsException.getExceptionByParameterName(WMS_EXCEPTION_CODE);

    std::string exceptionCode;
    std::string exceptionText;

    if (e != nullptr)
    {
      exceptionCode = e->getParameterValue(WMS_EXCEPTION_CODE);
      if (!isdebug)
        exceptionText = e->getWhat();
      else
        exceptionText = e->getStackTrace();
    }
    else
    {
      exceptionCode = "InvalidExceptionCode";
      exceptionText = wmsException.getWhat();
    }

    hash["exception_code"] = exceptionCode;
    hash["exception_text"] = exceptionText;

    auto wms_exception_template = getTemplate("wms_exception");

    std::stringstream tmpl_ss;
    std::stringstream logstream;
    wms_exception_template->process(hash, tmpl_ss, logstream);

    return tmpl_ss.str();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS query
 */
// ----------------------------------------------------------------------
WMSQueryStatus Dali::Plugin::wmsQuery(Spine::Reactor & /* theReactor */,
                                      State &theState,
                                      const Spine::HTTP::Request &theRequest,
                                      Spine::HTTP::Response &theResponse)
{
  try
  {
    // WMS-functionality is handled by adjusting requests parameters accordingly
    // Make a copy here since incoming requests are const

    auto thisRequest = theRequest;

    // Establish debugging related variables

    bool print_hash = Spine::optional_bool(thisRequest.getParameter("printhash"), false);

    bool print_json = Spine::optional_bool(thisRequest.getParameter("printjson"), false);

    bool isdebug = Spine::optional_bool(thisRequest.getParameter("debug"), false);

    Product product;
    WMS::WMSRequestType requestType(WMS::wmsRequestType(thisRequest));

    try
    {
      if (requestType == WMS::WMSRequestType::NOT_A_WMS_REQUEST)
      {
        Spine::Exception ex(BCP, "Not a WMS request!");
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
        auto msg = parseWMSException(ex, isdebug);
        formatResponse(msg, "xml", thisRequest, theResponse, theState.useTimer());
        throw ex;
      }

      if (requestType == WMS::WMSRequestType::GET_CAPABILITIES)
      {
        auto msg = itsWMSGetCapabilities->response(thisRequest, *itsQEngine, *itsWMSConfig);
        formatResponse(msg, "xml", thisRequest, theResponse, theState.useTimer());
        return WMSQueryStatus::OK;
      }

      if (requestType == WMS::WMSRequestType::GET_FEATURE_INFO)
      {
        Spine::Exception exception(BCP, "GetFeatureInfo not supported!");
        exception.addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED);
        auto msg = parseWMSException(exception, isdebug);
        formatResponse(msg, "xml", thisRequest, theResponse, theState.useTimer());
        return WMSQueryStatus::OK;
      }

      Json::Value json;

      if (requestType == WMS::WMSRequestType::GET_MAP)
      {
        WMS::WMSGetMap wmsGetMapRequest(*itsWMSConfig);

        // This request is a GetMap request
        // Validate authorizations

#ifndef WITHOUT_AUTHENTICATION
        bool has_access = false;
        if (itsConfig.authenticate())
          has_access = itsWMSConfig->validateGetMapAuthorization(thisRequest);
        else
          has_access = true;

        if (!has_access)
        {
          // Send 403 FORBIDDEN
          theResponse.setStatus(Spine::HTTP::Status::forbidden, true);
          return WMSQueryStatus::FORBIDDEN;
        }
#endif
        wmsGetMapRequest.parseHTTPRequest(*itsQEngine, thisRequest);

        json = wmsGetMapRequest.json();
      }
      else if (requestType == WMS::WMSRequestType::GET_LEGEND_GRAPHIC)
      {
        WMS::WMSGetLegendGraphic wmsGetLegendGraphic(*itsWMSConfig);
        wmsGetLegendGraphic.parseHTTPRequest(*itsQEngine, thisRequest);
        json = wmsGetLegendGraphic.json();
        if (itsWMSConfig->getLegendGraphicSettings().expires > 0)
        {
          auto tmp = boost::posix_time::second_clock::universal_time() +
                     boost::posix_time::seconds(itsWMSConfig->getLegendGraphicSettings().expires);
          theState.updateExpirationTime(tmp);
        }
      }

      if (requestType == WMS::WMSRequestType::GET_MAP)
      {
        std::string styleName = *(theRequest.getParameter("STYLES"));
        SmartMet::Plugin::WMS::useStyle(json, styleName);
      }

      // Define the customer
      std::string customer =
          Spine::optional_string(thisRequest.getParameter("customer"), itsConfig.defaultCustomer());
      theState.setCustomer(customer);

      if (theState.getCustomer().empty())
      {
        Spine::Exception ex(BCP, "Customer setting is empty!");
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
        auto msg = parseWMSException(ex, isdebug);
        formatResponse(msg, "xml", thisRequest, theResponse, theState.useTimer());
        throw ex;
      }

      // Set WMS product defaults before preprocessing starts

      if (itsWMSConfig->getMargin() != 0)
      {
        Json::Value nulljson;
        auto xmargin = json.get("xmargin", nulljson);
        if (xmargin.isNull())
          json["xmargin"] = itsWMSConfig->getMargin();
        auto ymargin = json.get("ymargin", nulljson);
        if (ymargin.isNull())
          json["ymargin"] = itsWMSConfig->getMargin();
      }

      // Preprocess

      std::string customer_root =
          (itsConfig.rootDirectory(theState.useWms()) + "/customers/" + customer);

      std::string layers_root = customer_root + "/layers/";

      Spine::JSON::preprocess(
          json, itsConfig.rootDirectory(theState.useWms()), layers_root, itsJsonCache);

      Spine::JSON::dereference(json);

      Spine::JSON::expand(json, thisRequest.getParameterMap(), "", false);

      // Debugging

      if (print_json)
      {
        Json::StyledWriter writer;
        std::cout << "Expanded Spine::JSON:" << std::endl << writer.write(json) << std::endl;
      }

      // And initialize the product specs from the JSON
      product.init(json, theState, itsConfig);

      if (requestType == WMS::WMSRequestType::GET_LEGEND_GRAPHIC)
      {
        std::string layerName = *(theRequest.getParameter("LAYER"));
        std::string styleName = *(theRequest.getParameter("STYLE"));
        if (styleName.empty())
          styleName = "default";
        itsWMSConfig->getLegendGraphic(layerName, styleName, product, theState);
        // getLegendGraphic-function sets width and height, but if width & height is given in
        // request override values
        std::string xsize = Fmi::to_string(*product.width);
        std::string ysize = Fmi::to_string(*product.height);
        if (theRequest.getParameter("WIDTH"))
        {
          xsize = *(thisRequest.getParameter("projection.xsize"));
          product.width = Fmi::stoi(xsize);
        }
        if (theRequest.getParameter("HEIGHT"))
        {
          ysize = *(thisRequest.getParameter("projection.ysize"));
          product.height = Fmi::stoi(ysize);
        }
        thisRequest.setParameter("projection.xsize", xsize);
        thisRequest.setParameter("projection.ysize", ysize);
      }
    }
    catch (...)
    {
      Spine::Exception ex(BCP, "Operation failed!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      auto msg = parseWMSException(ex, isdebug);
      formatResponse(msg, "xml", thisRequest, theResponse, theState.useTimer());
      throw ex;
    }

    // Format can no longer be changed by anything, provide the info to layers
    theState.setType(product.type);

    // Calculate hash for the product

    auto product_hash = product.hash_value(theState);

    // If request was ETag request, respond accordingly
    if (thisRequest.getHeader("X-Request-ETag"))
    {
      theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));
      theResponse.setHeader("Content-Type", mimeType(product.type));
      theResponse.setStatus(Spine::HTTP::Status::no_content);
      return WMSQueryStatus::OK;
    }

    auto obj = itsImageCache->find(product_hash);

#ifdef MYDEBUG
    std::cout << "Product hash_value = " << product_hash << std::endl;
#endif

    if (obj)
    {
#ifdef MYDEBUG
      std::cout << "\treturning cached image" << std::endl;
#endif
      theResponse.setHeader("Content-Type", mimeType(product.type));

      // For frontend caching
      theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));

      theResponse.setContent(obj);
      return WMSQueryStatus::OK;
    }

    if (!product.svg_tmpl)
      product.svg_tmpl = itsConfig.defaultTemplate(product.type);

    auto tmpl = getTemplate(*product.svg_tmpl);

    // Build the response CDT
    CTPP::CDT hash(CTPP::CDT::HASH_VAL);
    if (requestType == WMS::WMSRequestType::GET_LEGEND_GRAPHIC)
      hash["legend"] = "true";

    product.generate(hash, theState);

    // Build the template
    std::ostringstream output, log;
    try
    {
      std::string report = "Template processing finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
      tmpl->process(hash, output, log);
    }
    catch (...)
    {
      Spine::Exception ex(
          BCP, "Error in processing the template '" + *product.svg_tmpl + "'!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      auto msg = parseWMSException(ex, isdebug);
      formatResponse(msg, "xml", thisRequest, theResponse, theState.useTimer());
      throw ex;
    }

    if (print_hash)
      std::cout << "Generated CDT:" << std::endl << hash.RecursiveDump() << std::endl;

    formatResponse(output.str(),
                   product.type,
                   thisRequest,
                   theResponse,
                   theState.useTimer(),
                   product,
                   product_hash);
    return WMSQueryStatus::OK;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
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
  delete us;
}
