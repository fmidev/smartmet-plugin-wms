#include "Mime.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Establish format from mime type
 */
// ----------------------------------------------------------------------

std::string demimetype(const std::string& theMimeType)
{
  if (theMimeType == "image/png")
    return "png";
  if (theMimeType == "application/pdf")
    return "pdf";
  if (theMimeType == "application/postscript")
    return "ps";
  if (theMimeType == "image/svg+xml")
    return "svg";
  if (theMimeType == "application/json")
    return "json";
  if (theMimeType == "application/geo+json")
    return "geojson";
  if (theMimeType == "application/vnd.google-earth.kml+xml")
    return "kml";
  if (theMimeType == "cnf")
    return "cnf";

  throw Fmi::Exception(BCP, "Unknown mime type requested: '" + theMimeType + "'");
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the mime type for the product
 */
// ----------------------------------------------------------------------

std::string mimeType(const std::string& theType)
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
  if (theType == "json" || theType == "application/json")
    return "application/json";
  if (theType == "cnf")
    return "application/json";

  throw Fmi::Exception(BCP, "Unknown image format '" + theType + "'");
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
