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
#ifndef WITHOUT_AUHTENTICATION
  bool authenticate() const;
#endif

#ifndef WITHOUT_OBSERVATION
  bool obsEngineDisabled() const { return itsObsEngineDisabled; }
#endif

 private:
  libconfig::Config itsConfig;
  std::string itsDefaultUrl = "/dali";
  std::string itsDefaultTemplate = "svg";
  std::string itsDefaultCustomer = "fmi";
  std::string itsDefaultModel = "pal_skandinavia";
  std::string itsDefaultLanguage = "en";

  std::string itsTemplateDirectory = "/etc/smartmet/plugins/dali/templates";
  std::string itsRootDirectory;

  std::string itsFilesystemCacheDirectory = "/var/smartmet/imagecache";

  unsigned long long itsMaxMemoryCacheSize = 104857600;      // 100 MB
  unsigned long long itsMaxFilesystemCacheSize = 209715200;  // 200 MB

  std::string itsWmsUrl = "/wms";
  std::string itsWmsRootDirectory;
  std::string itsWmsVersions;
  std::string itsWmsMapFormats;

  std::set<std::string> itsRegularAttributes;
  std::set<std::string> itsPresentationAttributes;

  bool itsQuiet = false;

#ifndef WITHOUT_AUHTENTICATION
  bool itsAuthenticate = true;
#endif

#ifndef WITHOUT_OBSERVATION
  bool itsObsEngineDisabled = false;
#endif

};  // class Config

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
