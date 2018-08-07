// ======================================================================

#include "TemplateFactory.h"
#include <boost/filesystem/operations.hpp>
#include <boost/make_shared.hpp>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Get a thread specific template formatter for given template
 */
// ----------------------------------------------------------------------

SharedFormatter TemplateFactory::get(const boost::filesystem::path& theFilename) const
{
  try
  {
    if (theFilename.empty())
      throw Spine::Exception(BCP, "TemplateFactory: Cannot use empty templates");

    // Make sure thread specific map is initialized
    if (itsTemplates.get() == nullptr)
      itsTemplates.reset(new TemplateMap);

    // Find out if there is a formatter which is up to date

    auto& tmap = *itsTemplates;

    const auto& tinfo = tmap.find(theFilename);

    const std::time_t modtime = boost::filesystem::last_write_time(theFilename);

    // Use cached template if it is up to date
    if (tinfo != tmap.end())
      if (tinfo->second.modtime == modtime)
        return tinfo->second.formatter;

    // Initialize a new formatter

    TemplateInfo newinfo;
    newinfo.modtime = modtime;
    newinfo.formatter = boost::make_shared<Fmi::TemplateFormatter>();
    newinfo.formatter->load_template(theFilename.c_str());

    // Cache the new formatter
    tmap.insert(std::make_pair(theFilename, newinfo));

    return newinfo.formatter;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
