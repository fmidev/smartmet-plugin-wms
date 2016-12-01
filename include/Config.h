// ======================================================================
/*!
 * \brief Configuration file API
 */
// ======================================================================

#pragma once

#include <libconfig.h++>
#include <boost/utility.hpp>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config : private boost::noncopyable
{
 public:
  Config() = delete;
  Config(const std::string& configfile);

  const std::string& defaultUrl() const;
  const std::string& defaultTemplate() const;
  const std::string& defaultCustomer() const;
  const std::string& defaultModel() const;
  const std::string& defaultLanguage() const;

  const std::string& templateDirectory() const;

  const std::set<std::string>& regularAttributes() const;
  const std::set<std::string>& presentationAttributes() const;
  bool isValidAttribute(const std::string& theName) const;

  const std::string& wmsUrl() const;
  const std::string& wmsVersions() const;
  const std::string& wmsMapFormats() const;

  const std::string& rootDirectory(bool theWmsMode) const;

  unsigned long long maxMemoryCacheSize() const;

  unsigned long long maxFilesystemCacheSize() const;

  const std::string& filesystemCacheDirectory() const;

  const libconfig::Config& getConfig() const { return itsConfig; }
  bool quiet() const;
  bool authenticate() const;

 private:
  libconfig::Config itsConfig;
  std::string itsDefaultUrl;
  std::string itsDefaultTemplate;
  std::string itsDefaultCustomer;
  std::string itsDefaultModel;
  std::string itsDefaultLanguage;

  std::string itsTemplateDirectory;
  std::string itsRootDirectory;

  std::string itsFilesystemCacheDirectory;

  unsigned long long itsMaxMemoryCacheSize;
  unsigned long long itsMaxFilesystemCacheSize;

  std::string itsWmsUrl;
  std::string itsWmsRootDirectory;
  std::string itsWmsVersions;
  std::string itsWmsMapFormats;

  std::set<std::string> itsRegularAttributes;
  std::set<std::string> itsPresentationAttributes;

  bool itsQuiet;
  bool itsAuthenticate;
};  // class Config

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
