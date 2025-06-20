// ======================================================================
/*!
 * \brief Configuration file API
 */
// ======================================================================

#pragma once

#include <libconfig.h++>
#include <map>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config
{
 public:
  struct UnitConversion
  {
    double multiplier = 1;
    double offset = 0;
  };

  ~Config() = default;
  explicit Config(const std::string& configfile);

  Config() = delete;
  Config(const Config& other) = delete;
  Config& operator=(const Config& other) = delete;
  Config(Config&& other) = delete;
  Config& operator=(Config&& other) = delete;

  const std::string& defaultUrl() const;
  const std::string& defaultTemplate() const;
  const std::string& defaultCustomer() const;
  const std::string& defaultModel() const;
  const std::string& defaultLanguage() const;
  const std::set<std::string>& languages() const;

  const std::string& templateDirectory() const;
  const std::string& primaryForecastSource() const;

  const std::set<std::string>& regularAttributes() const;
  const std::set<std::string>& presentationAttributes() const;
  bool isValidAttribute(const std::string& theName) const;

  const std::string& wmsUrl() const;

  const std::string& rootDirectory(bool theWmsFlag) const;

  unsigned long long maxMemoryCacheSize() const;
  unsigned long long maxFilesystemCacheSize() const;
  unsigned int styleSheetCacheSize() const;

  unsigned int maxImageSize() const;
  unsigned int maxWMSLayers() const;

  const std::string& filesystemCacheDirectory() const;

  unsigned maxHeatmapPoints() const;

  const libconfig::Config& getConfig() const { return itsConfig; }
  bool quiet() const;

  std::string defaultTemplate(const std::string& theType) const;

  double defaultPrecision(const std::string& theType, const std::string& theName) const;

  UnitConversion unitConversion(const std::string& theUnitConversion) const;

#ifndef WITHOUT_AUHTENTICATION
  bool authenticate() const;
#endif

#ifndef WITHOUT_OBSERVATION
  bool obsEngineDisabled() const { return itsObsEngineDisabled; }
#endif
  bool gridEngineDisabled() const { return itsGridEngineDisabled; }

 private:
  libconfig::Config itsConfig;
  std::string itsDefaultUrl = "/dali";
  std::string itsDefaultTemplate = "svg";
  std::string itsDefaultCustomer = "fmi";
  std::string itsDefaultModel = "pal_skandinavia";
  std::string itsDefaultLanguage = "en";
  std::string itsPrimaryForecastSource = "querydata";
  std::set<std::string> itsLanguages;

  std::string itsTemplateDirectory = "/usr/share/smartmet/wms";
  std::string itsRootDirectory;

  std::string itsFilesystemCacheDirectory = "/var/smartmet/imagecache";

  unsigned long long itsMaxMemoryCacheSize = 104857600;      // 100 MB
  unsigned long long itsMaxFilesystemCacheSize = 209715200;  // 200 MB
  unsigned int itsStyleSheetCacheSize = 1000;                // 1000 objects

  unsigned int itsMaxImageSize = 20 * 1024 * 1024;  // 20M pixels
  unsigned int itsMaxWMSLayers = 10;                // no more than 10 layers, ddos protection
  unsigned itsMaxHeatmapPoints = 2000 * 2000;

  std::string itsWmsUrl = "/wms";
  std::string itsWmsRootDirectory;

  std::set<std::string> itsRegularAttributes;
  std::set<std::string> itsPresentationAttributes;

  std::map<std::string, std::string> itsDefaultTemplates;  // data type to template name

  std::map<std::string, double> itsDefaultPrecisions;  // path precision for SVG etc

  bool itsQuiet = false;

#ifndef WITHOUT_AUHTENTICATION
  bool itsAuthenticate = true;
#endif

#ifndef WITHOUT_OBSERVATION
  bool itsObsEngineDisabled = false;
#endif
  bool itsGridEngineDisabled = false;

  std::map<std::string, UnitConversion> itsUnitConversionMap;

};  // class Config

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
