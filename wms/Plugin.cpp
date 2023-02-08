// ======================================================================
/*!
 * \brief SmartMet Dali plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "CaseInsensitiveComparator.h"
#include "Hash.h"
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
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/move/make_unique.hpp>
#include <boost/shared_ptr.hpp>
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

    throw Fmi::Exception(
        BCP, "Attack IRI detected, relative paths upwards are not safe: '" + theName + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Init margins from config defaults if necessary
 */
// ----------------------------------------------------------------------

void init_wms_margins(Json::Value &json, int default_margin)
{
  if (default_margin == 0)
    return;

  Json::Value nulljson;
  auto xmargin = json.get("xmargin", nulljson);
  if (xmargin.isNull())
    json["xmargin"] = default_margin;
  auto ymargin = json.get("ymargin", nulljson);
  if (ymargin.isNull())
    json["ymargin"] = default_margin;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set Legend expiration time if so configured
 */
// ----------------------------------------------------------------------

void update_legend_expiration(State &theState, int theExpirationTime)
{
  if (theExpirationTime > 0)
  {
    auto tmp = boost::posix_time::second_clock::universal_time() +
               boost::posix_time::seconds(theExpirationTime);
    theState.updateExpirationTime(tmp);
  }
}

std::string get_json_element_value(const Json::Value &json, const std::string &keyStr)
{
  std::string ret;

  std::vector<std::string> keys;
  boost::algorithm::split(keys, keyStr, boost::is_any_of("."), boost::token_compress_on);

  const Json::Value *jsonPtr = &json;
  for (const auto &key : keys)
  {
    if (jsonPtr->isMember(key))
    {
      jsonPtr = &((*jsonPtr)[key]);
      if (key == keys.back())
        ret = jsonPtr->asString();
    }
  }

  return ret;
}

void rename_json_element(const Json::Value &json,
                         const std::string &keyStr,
                         const std::string &postfix)
{
  std::vector<std::string> keys;

  boost::algorithm::split(keys, keyStr, boost::is_any_of("."), boost::token_compress_on);

  Json::Value nulljson;
  const Json::Value *to = &json;
  bool found = false;
  for (const auto &key : keys)
  {
    found = false;
    if (to->isMember(key))
    {
      Json::Value::Members members = to->getMemberNames();
      to = &((*to)[key]);
      found = true;
    }
  }

  if (found)
  {
    auto *tototo = const_cast<Json::Value *>(to);
    *tototo = Json::Value(to->asString() + postfix);
  }
}

Json::Value merge_layers(const std::vector<Json::Value> &layers)
{
  Json::Value nulljson;

  unsigned int suffix = 1;

  for (const auto &layer : layers)
  {
    const auto &views = layer["views"];
    if (!views.isNull() && views.isArray())
    {
      for (const auto &view : views)
      {
        // Rename qid and attributes.id of the view
        std::string suffixString = Fmi::to_string(suffix);
        rename_json_element(view, "qid", suffixString);
        rename_json_element(view, "attributes.id", suffixString);

        // Rename qid and attributes.id of view's layers
        const auto &viewlayers = view["layers"];
        for (const auto &viewlayer : viewlayers)
        {
          rename_json_element(viewlayer, "qid", suffixString);
          rename_json_element(viewlayer, "attributes.id", suffixString);

          // Rename qid and attributes.id of view's sublayers
          const auto &viewsublayers = viewlayer["layers"];
          for (const auto &viewsublayer : viewsublayers)
          {
            rename_json_element(viewsublayer, "qid", suffixString);
            rename_json_element(viewsublayer, "attributes.id", suffixString);
          }
        }
        suffix++;
      }
    }
  }

  // Merge layers. Set layer #0 to the bottom and merge rest of layers on top of that.
  Json::Value ret = layers[0];
  Json::Value &retViews = ret["views"];

  // Set retDefsStyles to point to merged layer's styles attribute
  // Set retDefsLayers to point to merged layer's layers vector
  Json::Value *retDefs = nullptr;
  if (ret.isMember("defs"))
  {
    retDefs = &(ret["defs"]);
    if (!retDefs->isMember("styles"))
      (*retDefs)["styles"] = Json::Value(Json::objectValue);
    if (!retDefs->isMember("layers"))
      (*retDefs)["layers"] = Json::Value(Json::arrayValue);
  }
  else
  {
    ret["defs"] = Json::Value(Json::objectValue);
    retDefs = &(ret["defs"]);
    (*retDefs)["styles"] = Json::Value(Json::objectValue);
    (*retDefs)["layers"] = Json::Value(Json::arrayValue);
  }
  Json::Value &retDefsStyles = (*retDefs)["styles"];
  Json::Value &retDefsLayers = (*retDefs)["layers"];

  // Styles on WMS layer (can be changed in URL)
  if (!ret.isMember("styles"))
    ret["styles"] = Json::Value(Json::arrayValue);
  Json::Value &retStyles = ret["styles"];

  if (!ret.isMember("refs"))
    ret["refs"] = Json::Value(Json::objectValue);
  Json::Value &retRefs = ret["refs"];

  // Store ids of layers in defs-section of first layer
  std::set<std::string> defsLayerIdSet;
  for (const auto &layer : retDefsLayers)
  {
    std::string layerId = get_json_element_value(layer, "attributes.id");
    if (!layerId.empty())
      defsLayerIdSet.insert(layerId);
  }

  // Iterate rest of the layers in defs-section
  for (unsigned int i = 1; i < layers.size(); i++)
  {
    const Json::Value &fromLayer = layers[i];
    // Merge defs
    if (fromLayer.isMember("defs"))
    {
      Json::Value fromDefs = fromLayer.get("defs", nulljson);
      // Merge layer styles
      if (fromDefs.isMember("styles"))
      {
        Json::Value::Members currentStyleNames = retDefsStyles.getMemberNames();
        std::set<std::string> currentStyleNameSet(currentStyleNames.begin(),
                                                  currentStyleNames.end());
        const Json::Value &fromDefsStyles = fromDefs["styles"];
        Json::Value::Members fromStyleMemberNames = fromDefsStyles.getMemberNames();
        for (const auto &stylename : fromStyleMemberNames)
        {
          // If style with same name does not exist add it
          if (currentStyleNameSet.find(stylename) == currentStyleNameSet.end())
            (retDefsStyles)[stylename] = fromDefsStyles[stylename];
        }
      }
      // Merge layers inside defs, if layer with the same id exist dont merge
      if (fromDefs.isMember("layers"))
      {
        const auto &defsLayers = fromDefs["layers"];
        for (const auto &layer : defsLayers)
        {
          std::string layerId = get_json_element_value(layer, "attributes.id");
          if (layerId.empty() || defsLayerIdSet.find(layerId) == defsLayerIdSet.end())
          {
            retDefsLayers.append(layer);
            defsLayerIdSet.insert(layerId);
          }
        }
      }
    }

    // Merge refs
    if (fromLayer.isMember("refs"))
    {
      Json::Value fromRefs = fromLayer.get("refs", nulljson);
      Json::Value::Members fromRefNames = fromRefs.getMemberNames();
      Json::Value::Members currentRefNames = retRefs.getMemberNames();
      std::set<std::string> currentRefNameSet(currentRefNames.begin(), currentRefNames.end());
      for (const auto &refname : fromRefNames)
      {
        // If ref with same name does not exist add it
        if (currentRefNameSet.find(refname) == currentRefNameSet.end())
          (retRefs)[refname] = fromRefs[refname];
      }
    }

    // Merge styles
    if (fromLayer.isMember("styles"))
    {
      Json::Value fromStyles = fromLayer.get("styles", nulljson);
      if (!fromStyles.isNull() && fromStyles.isArray())
      {
        for (const auto &fromStyle : fromStyles)
          retStyles.append(fromStyle);
      }
    }

    // Merge views, just append one after another
    auto fromViews = fromLayer.get("views", nulljson);
    if (!fromViews.isNull() && fromViews.isArray())
    {
      for (const auto &fromView : fromViews)
        retViews.append(fromView);
    }
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \print Information on product parameters
 */
// ----------------------------------------------------------------------

void print(const ParameterInfos &infos)
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
}  // namespace

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
    if (width < 20 || width > 10000)
    {
      throw Fmi::Exception(BCP, "Invalid 'width' value '" + std::to_string(width) + "'!");
    }

    int height = Spine::optional_int(theRequest.getParameter("height"), 1000);
    if (height < 20 || height > 10000)
    {
      throw Fmi::Exception(BCP, "Invalid 'height' value '" + std::to_string(height) + "'!");
    }

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
          boost::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));
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
      std::string report = "Product::generate finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
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
      std::string report = "Template processing finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
        mytimer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
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
      std::string report = "svg_to_" + theType + " finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (usetimer)
        mytimer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

      boost::shared_ptr<std::string> buffer;
      if (theType == "png")
        buffer = boost::make_shared<std::string>(Giza::Svg::topng(theSvg, theProduct.png.options));
      else if (theType == "webp")
        buffer = boost::make_shared<std::string>(Giza::Svg::towebp(theSvg));
      else if (theType == "pdf")
        buffer = boost::make_shared<std::string>(Giza::Svg::topdf(theSvg));
      else if (theType == "ps")
        buffer = boost::make_shared<std::string>(Giza::Svg::tops(theSvg));
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
            firstMessage = firstMessage.substr(0, 300);
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

      boost::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));

      const ptime t_now = boost::posix_time::second_clock::universal_time();
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
          theResponse.setHeader("Expires", tformat->format(t_now + boost::posix_time::hours(1)));
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
      firstMessage = firstMessage.substr(0, 300);
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

    itsImageCache = boost::movelib::make_unique<ImageCache>(itsConfig.maxMemoryCacheSize(),
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
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(itsConfig,
                                                                 itsJsonCache,
                                                                 itsQEngine,
                                                                 authEngine,
                                                                 itsObsEngine,
                                                                 itsGisEngine,
                                                                 itsGridEngine);
#else
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, authEngine, itsGisEngine);
#endif
    }
    else
    {
#ifndef WITHOUT_OBSERVATION
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, nullptr, itsObsEngine, itsGisEngine, itsGridEngine);
#else
      itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
          itsConfig, itsJsonCache, itsQEngine, nullptr, itsGisEngine);
#endif
    }

#else
#ifndef WITHOUT_OBSERVATION
    itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
        itsConfig, itsJsonCache, itsQEngine, itsObsEngine, itsGisEngine, itsGridEngine);
#else
    itsWMSConfig = boost::movelib::make_unique<WMS::WMSConfig>(
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

    if (!boost::filesystem::exists(product_path))
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

    Spine::JSON::replaceReferences(json, theRequest.getParameterMap());

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

    Spine::JSON::expand(json, theRequest.getParameterMap(), "", false);

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
                                    bool theWmsFlag) const
{
  if (theCustomer.empty() || theFileName.empty())
    return "";

  try
  {
    std::string file_path;
    std::string filename = theFileName;

    // 1) If file name starts with '/' search from wms-root
    // 2) else check existence of file first from customer-path then from wms-root
    // 3) In wms-root check in the following order
    // 3.1) wms-root
    // 3.2) wms-root/resources/layers/<theSubDir> except for filters check
    // wms-root/resources/filters 3.3) wms-root/resources/<theSubDir> 3.4) wms-root/resources

    if (filename[0] != '/')
    {
      file_path = (itsConfig.rootDirectory(theWmsFlag) + "/customers/" + check_attack(theCustomer) +
                   theSubDir + check_attack(theFileName));
      if (!boost::filesystem::exists(file_path))
        filename.insert(filename.begin(), '/');
    }
    if (filename[0] == '/')
    {
      file_path = itsConfig.rootDirectory(theWmsFlag) + check_attack(filename);
      if (!boost::filesystem::exists(file_path))
      {
        if (theSubDir == "/filters/")
          file_path =
              itsConfig.rootDirectory(theWmsFlag) + "/resources/filters" + check_attack(filename);
        else
        {
          file_path = itsConfig.rootDirectory(theWmsFlag) + "/resources/layers" + theSubDir +
                      check_attack(filename);
          if (!boost::filesystem::exists(file_path))
            file_path = itsConfig.rootDirectory(theWmsFlag) + "/resources" + theSubDir +
                        check_attack(filename);
          if (!boost::filesystem::exists(file_path))
          {
            file_path = itsConfig.rootDirectory(theWmsFlag) + "/resources" + check_attack(filename);
          }
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

    std::string css_path = resolveFilePath(theCustomer, "/layers/", theCSS, theWmsFlag);

    return itsFileCache.get(css_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::string filter_path = resolveFilePath(theCustomer, "/filters/", theName, theWmsFlag);

    return itsFileCache.get(filter_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::string filter_path = resolveFilePath(theCustomer, "/filters/", theName, theWmsFlag);

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

    std::string marker_path = resolveFilePath(theCustomer, "/markers/", theName, theWmsFlag);

    return itsFileCache.get(marker_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::string marker_path = resolveFilePath(theCustomer, "/markers/", theName, theWmsFlag);

    return itsFileCache.last_modified(marker_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::string symbol_path = resolveFilePath(theCustomer, "/symbols/", theName, theWmsFlag);

    return itsFileCache.get(symbol_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::string symbol_path = resolveFilePath(theCustomer, "/symbols/", theName, theWmsFlag);

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

    std::string pattern_path = resolveFilePath(theCustomer, "/patterns/", theName, theWmsFlag);

    return itsFileCache.get(pattern_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::string pattern_path = resolveFilePath(theCustomer, "/patterns/", theName, theWmsFlag);

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

    std::string gradient_path = resolveFilePath(theCustomer, "/gradients/", theName, theWmsFlag);

    return itsFileCache.get(gradient_path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::string gradient_path = resolveFilePath(theCustomer, "/gradients/", theName, theWmsFlag);

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

std::string Dali::Plugin::getExceptionFormat(const std::string &theFormat) const
{
  const auto &supported_formats = itsWMSConfig->supportedWMSExceptions();
  if (supported_formats.find(theFormat) == supported_formats.end())
    return "xml";

  if (theFormat == "application/json")
    return "json";

  return "xml";
}

std::string Dali::Plugin::getCapabilityFormat(const std::string &theFormat) const
{
  const auto &supported_formats = itsWMSConfig->supportedWMSGetCapabilityFormats();
  if (supported_formats.find(theFormat) == supported_formats.end())
    return "xml";

  if (theFormat == "application/json")
    return "json";

  return "xml";
}

void Dali::Plugin::formatWmsExceptionResponse(Fmi::Exception &wmsException,
                                              const std::string &theFormat,
                                              bool isdebug,
                                              const Spine::HTTP::Request &theRequest,
                                              Spine::HTTP::Response &theResponse,
                                              bool usetimer)
{
  auto msg = parseWMSException(wmsException, theFormat, isdebug);
  formatResponse(msg, getExceptionFormat(theFormat), theRequest, theResponse, usetimer);
}

// ----------------------------------------------------------------------
/*!
 * \brief Parses WMS exception
 */
// ----------------------------------------------------------------------

std::string Dali::Plugin::parseWMSException(Fmi::Exception &wmsException,
                                            const std::string &theFormat,
                                            bool isdebug) const
{
  try
  {
    auto format = getExceptionFormat(theFormat);

    CTPP::CDT hash;

    const Fmi::Exception *e = wmsException.getExceptionByParameterName(WMS_EXCEPTION_CODE);

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

    auto tmpl_name = "wms_exception_" + format;
    auto wms_exception_template = getTemplate(tmpl_name);

    std::string tmpl;
    std::string log;
    wms_exception_template->process(hash, tmpl, log);
    return tmpl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Authenticate a query
 */
// ----------------------------------------------------------------------

bool Dali::Plugin::authenticate(const Spine::HTTP::Request &theRequest) const
{
#ifdef WITHOUT_AUTHENTICATION
  return true;
#else
  if (itsConfig.authenticate())
    return itsWMSConfig->validateGetMapAuthorization(theRequest);
  return true;
#endif
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
    WMS::WMSRequestType requestType = WMS::wmsRequestType(theRequest);

    // Handle common errors immediately

    if (requestType == WMS::WMSRequestType::NOT_A_WMS_REQUEST)
    {
      Fmi::Exception ex(BCP, ERROR_NOT_WMS_REQUEST);
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, theRequest, theResponse);
    }

    if (requestType == WMS::WMSRequestType::GET_FEATURE_INFO)
    {
      Fmi::Exception ex(BCP, ERROR_GETFEATUREINFO_NOT_SUPPORTED);
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED);
      return handleWmsException(ex, theState, theRequest, theResponse);
    }

    // Handle GetCapabilities separately

    if (requestType == WMS::WMSRequestType::GET_CAPABILITIES)
      return wmsGetCapabilitiesQuery(theState, theRequest, theResponse);

    // Authorize the request

    if (!authenticate(theRequest))
    {
      theResponse.setStatus(Spine::HTTP::Status::forbidden, true);
      return WMSQueryStatus::FORBIDDEN;  // 403 FORBIDDEN
    }

    if (requestType == WMS::WMSRequestType::GET_MAP)
      return wmsGetMapQuery(theState, theRequest, theResponse);

    if (requestType == WMS::WMSRequestType::GET_LEGEND_GRAPHIC)
      return wmsGetLegendGraphicQuery(theState, theRequest, theResponse);

    Fmi::Exception ex(BCP, ERROR_NOT_WMS_REQUEST);
    ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS GetMap query
 */
// ----------------------------------------------------------------------

WMSQueryStatus Dali::Plugin::wmsGetMapQuery(State &theState,
                                            const Spine::HTTP::Request &theRequest,
                                            Spine::HTTP::Response &theResponse)
{
  try
  {
    // Establish debugging related variables

    const auto print_params = Spine::optional_bool(theRequest.getParameter("printparams"), false);
    const auto print_json = Spine::optional_bool(theRequest.getParameter("printjson"), false);
    const auto json_stage = Spine::optional_int(theRequest.getParameter("stage"), 0);
    const auto cnf_request = Spine::optional_string(theRequest.getParameter("format"), "") == "cnf";

    // WMS-functionality is handled by adjusting requests parameters accordingly
    // Make a copy here since incoming requests are const

    Product product;
    auto thisRequest = theRequest;

    // Catch other errors and handle them with handleWmsException
    try
    {
      WMS::WMSGetMap wmsGetMapRequest(*itsWMSConfig);
      wmsGetMapRequest.parseHTTPRequest(*itsQEngine, thisRequest);
      auto json_layers = wmsGetMapRequest.jsons();
      auto styles = wmsGetMapRequest.styles();

      // Process the JSON layers

      for (auto i = 0UL; i < json_layers.size(); i++)
      {
        auto &json = json_layers.at(i);
        const auto &style = styles.at(i);

        wmsPreprocessJSON(theState, thisRequest, json, cnf_request, json_stage);
        SmartMet::Plugin::WMS::useStyle(json, style);
      }

      // Merge multiple layers into one

      Json::Value json;
      if (json_layers.size() == 1)
        json = json_layers.back();
      else
        json = merge_layers(json_layers);

      // Set defaults if some settings are missing

      init_wms_margins(json, itsWMSConfig->getMargin());

      // Debugging

      if (print_json)
      {
        Json::StyledWriter writer;
        std::cout << fmt::format("Expanded Spine::JSON:\n{}\n", writer.write(json));
      }

      // Special product
      if (cnf_request)
      {
        theResponse.setHeader("Content-Type", mimeType("cnf"));
        Json::StyledWriter writer;
        theResponse.setContent(writer.write(json));
        return WMSQueryStatus::OK;
      }

      // Establish type forced by the query. Must not use 'thisRequest' here
      auto fmt = Spine::optional_string(theRequest.getParameter("format"), "image/svg+xml");
      theState.setType(demimetype(fmt));

      // And initialize the product specs from the JSON

      product.init(json, theState, itsConfig);

      // If the desired type is not defined in the JSON, the state object knows from earlier code
      // what format to output (HTTP request or default format), and we can not set the Product to
      // use it.

      if (product.type.empty())
        product.type = theState.getType();
    }
    catch (...)
    {
      Fmi::Exception ex(BCP, "Operation failed!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, thisRequest, theResponse);
    }

    if (print_params)
      print(product.getGridParameterInfo(theState));

    return wmsGenerateProduct(theState, thisRequest, theResponse, product);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS GetLegendGraphic query
 */
// ----------------------------------------------------------------------

WMSQueryStatus Dali::Plugin::wmsGetLegendGraphicQuery(State &theState,
                                                      const Spine::HTTP::Request &theRequest,
                                                      Spine::HTTP::Response &theResponse)
{
  try
  {
    // Establish debugging related variables

    const auto print_json = Spine::optional_bool(theRequest.getParameter("printjson"), false);
    const auto json_stage = Spine::optional_int(theRequest.getParameter("stage"), 0);
    const auto cnf_request = Spine::optional_string(theRequest.getParameter("format"), "") == "cnf";

    // WMS-functionality is handled by adjusting requests parameters accordingly
    // Make a copy here since incoming requests are const

    Product product;
    auto thisRequest = theRequest;

    // Catch other errors and handle them with handleWmsException
    try
    {
      Json::Value json;

      WMS::WMSGetLegendGraphic wmsGetLegendGraphic(*itsWMSConfig);
      wmsGetLegendGraphic.parseHTTPRequest(*itsQEngine, thisRequest);
      json = wmsGetLegendGraphic.json();
      update_legend_expiration(theState, itsWMSConfig->getLegendGraphicSettings().expires);

      // Process the JSON

      wmsPreprocessJSON(theState, thisRequest, json, cnf_request, json_stage);

      // Debugging

      if (print_json)
      {
        Json::StyledWriter writer;
        std::cout << fmt::format("Expanded Spine::JSON:\n{}\n", writer.write(json));
      }

      // Special product
      if (cnf_request)
      {
        theResponse.setHeader("Content-Type", mimeType("cnf"));
        Json::StyledWriter writer;
        theResponse.setContent(writer.write(json));
        return WMSQueryStatus::OK;
      }

      // Establish type forced by the query

      auto fmt = Spine::optional_string(theRequest.getParameter("format"), "application/svg+xml");
      theState.setType(demimetype(fmt));

      // And initialize the product specs from the JSON

      product.init(json, theState, itsConfig);

      // If the desired type is not defined in the JSON, the state object knows from earlier code
      // what format to output (HTTP request or default format), and we can not set the Product to
      // use it.

      if (product.type.empty())
        product.type = theState.getType();

      wmsPrepareGetLegendGraphicQuery(theState, thisRequest, product);
    }
    catch (...)
    {
      Fmi::Exception ex(BCP, "Operation failed!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, thisRequest, theResponse);
    }

    return wmsGenerateProduct(theState, thisRequest, theResponse, product);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS GetCapabilities query
 */
// ----------------------------------------------------------------------

WMSQueryStatus Dali::Plugin::wmsGetCapabilitiesQuery(State &theState,
                                                     const Spine::HTTP::Request &theRequest,
                                                     Spine::HTTP::Response &theResponse)
{
  try
  {
    std::string format = Spine::optional_string(theRequest.getParameter("format"), "xml");

    theState.updateExpirationTime(itsWMSConfig->getCapabilitiesExpirationTime());
    auto tmpl = getTemplate("wms_get_capabilities_" + getCapabilityFormat(format));
    auto msg = WMS::WMSGetCapabilities::response(tmpl, theRequest, *itsQEngine, *itsWMSConfig);
    formatResponse(msg, format, theRequest, theResponse, theState.useTimer());
    theState.updateExpirationTime(itsWMSConfig->getCapabilitiesExpirationTime());
    theState.updateModificationTime(itsWMSConfig->getCapabilitiesModificationTime());
    return WMSQueryStatus::OK;
  }
  catch (const Fmi::Exception &wmsException)
  {
    Fmi::Exception ex(
        BCP, ("Error in parsing GetCapabilities response! " + std::string(wmsException.what())));
    if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }
  catch (...)
  {
    Fmi::Exception ex(BCP, ("Error in parsing GetCapabilities response!"));
    if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Postprocess a successfully handled GetMap/GetLegendGraphic product
 */
// ----------------------------------------------------------------------

WMSQueryStatus Dali::Plugin::wmsGenerateProduct(State &theState,
                                                const Spine::HTTP::Request &theRequest,
                                                Spine::HTTP::Response &theResponse,
                                                Product &theProduct)
{
  // Calculate hash for the product

  std::size_t product_hash = 0;
  try
  {
    product_hash = theProduct.hash_value(theState);
  }
  catch (const Fmi::Exception &exception)
  {
    auto layers = *(theRequest.getParameter("LAYERS"));

    Fmi::Exception ex(
        BCP,
        "Error in calculating hash_value for layer '" + layers + "'! " + exception.what(),
        nullptr);
    ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }

  // We always return valid ETags

  if (product_hash != Fmi::bad_hash)
  {
    theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));

    // If request was an ETag request, we're done already

    if (theRequest.getHeader("X-Request-ETag"))
    {
      theResponse.setHeader("Content-Type", mimeType(theProduct.type));
      theResponse.setStatus(Spine::HTTP::Status::no_content);
      return WMSQueryStatus::OK;
    }
  }

  auto obj = itsImageCache->find(product_hash);

  if (obj)
  {
    theResponse.setHeader("Content-Type", mimeType(theProduct.type));
    theResponse.setContent(obj);
    return WMSQueryStatus::OK;
  }

  if (!theProduct.svg_tmpl)
    theProduct.svg_tmpl = itsConfig.defaultTemplate(theProduct.type);

  auto tmpl = getTemplate(*theProduct.svg_tmpl);

  // Build the response CDT
  WMS::WMSRequestType requestType = WMS::wmsRequestType(theRequest);
  CTPP::CDT hash(CTPP::CDT::HASH_VAL);
  if (requestType == WMS::WMSRequestType::GET_LEGEND_GRAPHIC)
    hash["legend"] = "true";

  try
  {
    theProduct.generate(hash, theState);
  }
  catch (...)
  {
    Fmi::Exception e(BCP, "Failed to generate product", nullptr);
    e.addParameter("URI", theRequest.getURI());
    e.addParameter("ClientIP", theRequest.getClientIP());
    e.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));
    const bool check_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
    e.addParameter("Apikey", (apikey ? *apikey : std::string("-")));
    e.printError();
  }

  // Build the template
  std::string output;
  std::string log;
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
    Fmi::Exception ex(
        BCP, "Error in processing the template '" + *theProduct.svg_tmpl + "'!", nullptr);
    if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }

  const auto print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);
  if (print_hash)
    std::cout << fmt::format("Generated CDT:\n{}\n", hash.RecursiveDump());

  formatResponse(output,
                 theProduct.type,
                 theRequest,
                 theResponse,
                 theState.useTimer(),
                 theProduct,
                 product_hash);
  return WMSQueryStatus::OK;
}

// ----------------------------------------------------------------------
/*!
 * \brief Alter the product to be a legend graphic query
 */
// ----------------------------------------------------------------------

void Dali::Plugin::wmsPrepareGetLegendGraphicQuery(const State &theState,
                                                   Spine::HTTP::Request &theRequest,
                                                   Product &product) const
{
  auto layerOpt = theRequest.getParameter("LAYER");
  if (!layerOpt)
  {
    throw Fmi::Exception(BCP, "Layer must be defined in GetLegendGraphic request")
        .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
  }
  std::string layerName = *layerOpt;

  // Style is optional.
  auto styleOpt = theRequest.getParameter("STYLE");
  std::string styleName = (styleOpt && !styleOpt->empty() ? *styleOpt : "default");

  // Default language from configuration file
  std::string language = itsConfig.defaultLanguage();
  // Language overwritten from product file
  if (product.language)
    language = *product.language;
  // Finally language overwritten from URL-parameter
  auto languageParam = theRequest.getParameter("LANGUAGE");
  if (languageParam)
    language = *languageParam;

  itsWMSConfig->getLegendGraphic(layerName, styleName, product, theState, language);

  // getLegendGraphic-function sets width and height, but if width & height is given in
  // request override values
  std::string xsize = Fmi::to_string(*product.width);
  std::string ysize = Fmi::to_string(*product.height);
  if (theRequest.getParameter("WIDTH"))
  {
    xsize = *(theRequest.getParameter("projection.xsize"));
    product.width = Fmi::stoi(xsize);
  }
  if (theRequest.getParameter("HEIGHT"))
  {
    ysize = *(theRequest.getParameter("projection.ysize"));
    product.height = Fmi::stoi(ysize);
  }
  theRequest.setParameter("projection.xsize", xsize);
  theRequest.setParameter("projection.ysize", ysize);
}

// ----------------------------------------------------------------------
/*!
 * \brief Preprocess product JSON
 */
// ----------------------------------------------------------------------

void Dali::Plugin::wmsPreprocessJSON(State &theState,
                                     const Spine::HTTP::Request &theRequest,
                                     Json::Value &theJson,
                                     bool isCnfRequest,
                                     int theStage)
{
  // Define the customer. Note that parseHTTPRequest may have set a customer
  // for the layer, so this needs to be done after parsing the request

  auto customer =
      Spine::optional_string(theRequest.getParameter("customer"), itsConfig.defaultCustomer());
  theState.setCustomer(customer);

  if (theState.getCustomer().empty())
    throw Fmi::Exception(BCP, ERROR_NO_CUSTOMER)
        .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);

  // Preprocess

  std::string customer_root =
      (itsConfig.rootDirectory(theState.useWms()) + "/customers/" + customer);

  std::string layers_root = customer_root + "/layers/";

  if (!isCnfRequest || (theStage == 0 || theStage > 1))
    Spine::JSON::preprocess(
        theJson, itsConfig.rootDirectory(theState.useWms()), layers_root, itsJsonCache);

  if (!isCnfRequest || (theStage == 0 || theStage > 2))
    Spine::JSON::dereference(theJson);

  if (!isCnfRequest || (theStage == 0 || theStage > 3))
  {
    // Do querystring replacements, but remove dangerous substitutions first
    auto params = theRequest.getParameterMap();
    params.erase("styles");
    Spine::JSON::expand(theJson, params, "", false);
  }
}

WMSQueryStatus Dali::Plugin::handleWmsException(Fmi::Exception &exception,
                                                State &theState,
                                                const Spine::HTTP::Request &theRequest,
                                                Spine::HTTP::Response &theResponse)
{
  // Console logging
  exception.addParameter("URI", theRequest.getURI());
  exception.addParameter("ClientIP", theRequest.getClientIP());

  const bool check_token = true;
  auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
  exception.addParameter("Apikey", (apikey ? *apikey : std::string("-")));

  auto quiet = theRequest.getParameter("quiet");
  if (!quiet || (*quiet == "0" || *quiet == "false"))
    exception.printError();

  WMS::CaseInsensitiveComparator cicomp;
  WmsExceptionFormat exceptionFormat = WmsExceptionFormat::XML;
  std::string exceptionFormatStr =
      Spine::optional_string(theRequest.getParameter("EXCEPTIONS"), "XML");
  if (cicomp(exceptionFormatStr, "INIMAGE"))
    exceptionFormat = WmsExceptionFormat::INIMAGE;
  else if (cicomp(exceptionFormatStr, "BLANK"))
    exceptionFormat = WmsExceptionFormat::BLANK;
  else if (cicomp(exceptionFormatStr, "JSON"))
    exceptionFormat = WmsExceptionFormat::JSON;

  std::string mapFormat = Spine::optional_string(theRequest.getParameter("format"), "xml");

  bool isdebug = Spine::optional_bool(theRequest.getParameter("debug"), false);

  if (exceptionFormat != WmsExceptionFormat::INIMAGE &&
      exceptionFormat != WmsExceptionFormat::BLANK)
  {
    if (exceptionFormat == WmsExceptionFormat::JSON)
      mapFormat = "application/json";
    formatWmsExceptionResponse(
        exception, mapFormat, isdebug, theRequest, theResponse, theState.useTimer());
    // Note: a simple throw is not sufficient, the exception object may not have been thrown
    throw exception;
  }

  boost::optional<std::string> width = theRequest.getParameter("WIDTH");
  boost::optional<std::string> height = theRequest.getParameter("HEIGHT");

  Json::Value json = getExceptionJson(exception.what(),
                                      mapFormat,
                                      exceptionFormat,
                                      Fmi::stoi(width ? *width : "0"),
                                      Fmi::stoi(height ? *height : "0"));

  try
  {
    Product product;
    // Initialize the product specs from the JSON
    product.init(json, theState, itsConfig);

    if (!product.svg_tmpl)
      product.svg_tmpl = itsConfig.defaultTemplate(product.type);

    auto tmpl = getTemplate(*product.svg_tmpl);

    // Build the response CDT
    CTPP::CDT hash(CTPP::CDT::HASH_VAL);

    product.generate(hash, theState);

    // Build the template
    std::string output;
    std::string log;
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
      Fmi::Exception ex(
          BCP, "Error in processing the template '" + *product.svg_tmpl + "'!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      formatWmsExceptionResponse(
          ex, mapFormat, isdebug, theRequest, theResponse, theState.useTimer());
      throw ex;
    }
    formatResponse(
        output, product.type, theRequest, theResponse, theState.useTimer(), product, Fmi::bad_hash);

    return WMSQueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value Dali::Plugin::getExceptionJson(const std::string &description,
                                           const std::string &mapFormat,
                                           WmsExceptionFormat format,
                                           unsigned int width,
                                           unsigned int height)
{
  std::string jsonStr;

  std::string errorString = "Error: " + description;
  SmartMet::Plugin::Dali::text_style_t textStyle;
  textStyle.fontsize = "26";
  SmartMet::Plugin::Dali::text_dimension_t textDimension =
      SmartMet::Plugin::Dali::getTextDimension(errorString, textStyle);

  unsigned int pictureWidth = (width > 0 ? width : textDimension.width + 20);
  unsigned int pictureHeight = (height > 0 ? height : textDimension.height + 10);

  jsonStr = "{";
  jsonStr += "\"title\": \"Error\",\n";
  jsonStr += "\"language\": \"fi\",\n";
  if (mapFormat.find("png") != std::string::npos)
    jsonStr += "\"type\": \"png\",\n";
  else if (mapFormat.find("webp") != std::string::npos)
    jsonStr += "\"type\": \"webp\",\n";
  else if (mapFormat.find("pdf") != std::string::npos)
    jsonStr += "\"type\": \"pdf\",\n";
  else
    jsonStr += "\"type\": \"svg\",\n";
  jsonStr += "\"projection\":\n";
  jsonStr += "{\n";
  jsonStr += "   \"crs\": \"data\",\n";
  jsonStr += "   \"xsize\": " + std::to_string(pictureWidth) + ",\n";
  jsonStr += "   \"ysize\": " + std::to_string(pictureHeight) + "\n";
  jsonStr += "},\n";
  jsonStr += "\"views\": [\n";
  jsonStr += "    {\n";
  jsonStr += "        \"qid\": \"v1\",\n";
  jsonStr += "        \"attributes\":\n";
  jsonStr += "        {\n";
  jsonStr += "            \"id\": \"view1\"\n";
  jsonStr += "        },\n";
  jsonStr += "        \"layers\": [\n";
  if (format == WmsExceptionFormat::INIMAGE)
  {
    jsonStr += "{\n";
    jsonStr += "	   \"qid\": \"tagi\",\n";
    jsonStr += "	   \"tag\": \"g\",\n";
    jsonStr += "	   \"layer_type\": \"tag\"\n";
    jsonStr += "},\n";
    jsonStr += "{";
    jsonStr += "     \"tag\": \"text\",\n";
    jsonStr += "     \"cdata\":\n";
    jsonStr += "     {\n";
    jsonStr += ("           \"en\": \"" + errorString + "\",\n");
    jsonStr += ("           \"fi\": \"" + errorString + "\"\n");
    jsonStr += "     },\n";
    jsonStr += "     \"attributes\":\n";
    jsonStr += "     {\n";
    jsonStr += "           \"x\": \"5\",\n";
    jsonStr += "           \"y\": \"" + std::to_string(textDimension.height) + "\",\n";
    jsonStr += "           \"font-family\": \"" + textStyle.fontfamily + "\",\n";
    jsonStr += "           \"font-weight\": \"" + textStyle.fontweight + "\",\n";
    jsonStr += "           \"font-size\": \"" + textStyle.fontsize + "\",\n";
    jsonStr += "           \"text-anchor\": \"start\"\n";
    jsonStr += "      }\n";
    jsonStr += "}\n";
  }
  jsonStr += "         ]\n";
  jsonStr += "    }\n";
  jsonStr += "       ]\n";
  jsonStr += "}\n";

  return SmartMet::Plugin::WMS::WMSLayer::parseJsonString(jsonStr);
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
