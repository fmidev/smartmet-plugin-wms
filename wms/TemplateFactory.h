// ======================================================================
/*!
 * \brief A factory for thread safe template formatting
 *
 * TODO: Should this be in macgyver instead of something as trivial
 *       as TemplateFormatterMT ?
 */
// ======================================================================

#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <macgyver/TemplateFormatter.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
using SharedFormatter = boost::shared_ptr<Fmi::TemplateFormatter>;

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
  using TemplateMap = std::map<boost::filesystem::path, TemplateInfo>;
  using Templates = boost::thread_specific_ptr<TemplateMap>;
  mutable Templates itsTemplates;

};  // class TemplateFactory

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
