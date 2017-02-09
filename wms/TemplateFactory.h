// ======================================================================
/*!
 * \brief A factory for thread safe template formatting
 *
 * TODO: Should this be in macgyver instead of something as trivial
 *       as TemplateFormatterMT ?
 */
// ======================================================================

#pragma once

#include <macgyver/TemplateFormatter.h>
#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
typedef boost::shared_ptr<Fmi::TemplateFormatter> SharedFormatter;

class TemplateFactory : public boost::noncopyable
{
 public:
  SharedFormatter get(const boost::filesystem::path& theFilename) const;

 private:
  struct TemplateInfo
  {
    std::time_t modtime;
    SharedFormatter formatter;

    TemplateInfo() : modtime(0), formatter() {}
  };

  // CT++ may not be thread safe - but using a thread specific
  // storage for cached copies makes using it thread safe
  typedef std::map<boost::filesystem::path, TemplateInfo> TemplateMap;
  typedef boost::thread_specific_ptr<TemplateMap> Templates;
  mutable Templates itsTemplates;

};  // class TemplateFactory

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
