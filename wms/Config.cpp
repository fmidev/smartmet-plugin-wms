// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/ConfigTools.h>
#include <spine/Exceptions.h>
#include <filesystem>
#include <stdexcept>

using std::string;

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
std::size_t parse_size(const libconfig::Setting& setting, const char* name)
{
  switch (setting.getType())
  {
    case libconfig::Setting::TypeInt:
      return static_cast<unsigned int>(setting);
    case libconfig::Setting::TypeInt64:
      return static_cast<unsigned long>(setting);
    case libconfig::Setting::TypeString:
      return Fmi::stosz(static_cast<const char*>(setting));
    default:
      throw Fmi::Exception(BCP, "Invalid type for size setting").addParameter("Setting", name);
  }
}

}  // namespace
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

Config::Config(const string& configfile)
{
  if (configfile.empty())
    return;

  try
  {
    // Enable sensible relative include paths
    std::filesystem::path p = configfile;
    p.remove_filename();
    itsConfig.setIncludeDir(p.c_str());

    itsConfig.readFile(configfile.c_str());
    Spine::expandVariables(itsConfig);

    // required parameters
    std::string root = itsConfig.lookup("root");
    std::string wmsroot = itsConfig.lookup("wms.root");
    itsRootDirectory = root;
    itsWmsRootDirectory = wmsroot;

    // optional parameters
    itsConfig.lookupValue("url", itsDefaultUrl);
    itsConfig.lookupValue("model", itsDefaultModel);
    itsConfig.lookupValue("language", itsDefaultLanguage);

    {
      std::string langs;
      itsConfig.lookupValue("languages", langs);
      boost::algorithm::split(itsLanguages, langs, boost::is_any_of(","));
      itsLanguages.insert(itsDefaultLanguage);
    }

    itsConfig.lookupValue("primaryForecastSource", itsPrimaryForecastSource);

    itsConfig.lookupValue("template", itsDefaultTemplate);
    itsConfig.lookupValue("templatedir", itsTemplateDirectory);
    itsConfig.lookupValue("customer", itsDefaultCustomer);

    itsConfig.lookupValue("css_cache_size", itsStyleSheetCacheSize);

    itsConfig.lookupValue("cache.directory", itsFilesystemCacheDirectory);

    const char* memory_bytes = "cache.memory_bytes";
    if (itsConfig.exists(memory_bytes))
      itsMaxMemoryCacheSize = parse_size(itsConfig.lookup(memory_bytes), memory_bytes);

    const char* filesystem_bytes = "cache.filesystem_bytes";
    if (itsConfig.exists(filesystem_bytes))
      itsMaxFilesystemCacheSize = parse_size(itsConfig.lookup(filesystem_bytes), filesystem_bytes);

    itsConfig.lookupValue("max_image_size", itsMaxImageSize);
    itsConfig.lookupValue("wms.max_layers", itsMaxWMSLayers);

    itsConfig.lookupValue("heatmap.max_points", itsMaxHeatmapPoints);

    itsConfig.lookupValue("wms.url", itsWmsUrl);
    itsConfig.lookupValue("wms.quiet", itsQuiet);

#ifndef WITHOUT_AUHTENTICATION
    itsConfig.lookupValue("authenticate", itsAuthenticate);
#endif

#ifndef WITHOUT_OBSERVATION
    itsConfig.lookupValue("observation_disabled", itsObsEngineDisabled);
#endif
    itsConfig.lookupValue("gridengine_disabled", itsGridEngineDisabled);

    // Default templates for various types

    {
      const auto& templates = itsConfig.lookup("templates");
      if (!templates.isGroup())
      {
        throw Fmi::Exception::Trace(BCP, "Configuration error!")
            .addParameter("Configuration file", configfile)
            .addDetail("Configured value of 'templates' must be a group");
      }

      for (int i = 0; i < templates.getLength(); ++i)
      {
        const auto& setting = templates[i];
        std::string name = setting.getName();
        std::string value = setting;
        itsDefaultTemplates[name] = value;
      }
    }

    // Default precisions for various layers

    if (itsConfig.exists("precision"))
    {
      const auto& precisions = itsConfig.lookup("precision");
      if (!precisions.isGroup())
      {
        throw Fmi::Exception::Trace(BCP, "Configuration error!")
            .addParameter("Configuration file", configfile)
            .addDetail("Configured value of 'precisions' must be a group");
      }

      for (int i = 0; i < precisions.getLength(); ++i)
      {
        const auto& setting = precisions[i];
        std::string name = setting.getName();
        double value = setting;
        itsDefaultPrecisions[name] = value;
      }
    }

    // Optional unit conversion definitions

    if (itsConfig.exists("unit_conversion"))
    {
      const auto& conversions = itsConfig.lookup("unit_conversion");
      if (!conversions.isGroup())
      {
        throw Fmi::Exception::Trace(BCP, "Configuration error!")
            .addParameter("Configuration file", configfile)
            .addDetail("Configured value of 'unit_conversions' must be an array");
      }
      for (int i = 0; i < conversions.getLength(); ++i)
      {
        const auto& conversion = conversions[i];
        if (!conversion.isGroup())
        {
          throw Fmi::Exception::Trace(BCP, "Configuration error!")
              .addParameter("Configuration file", configfile)
              .addDetail(
                  "Configured value of a unit_conversion must be a group containing "
                  "multiplier/offset");
        }

        UnitConversion uconversion;  // initialized to 1*x+0
        conversion.lookupValue("multiplier", uconversion.multiplier);
        conversion.lookupValue("offset", uconversion.offset);
        itsUnitConversionMap[conversion.getName()] = uconversion;
      }
    }

    // Store array of SVG attribute names into a set for looking up valid names
    {
      const auto& attributes = itsConfig.lookup("regular_attributes");
      if (!attributes.isArray())
      {
        throw Fmi::Exception::Trace(BCP, "Configuration error!")
            .addParameter("Configuration file", configfile)
            .addDetail("Configured value of 'regular_attributes' must be an array");
      }
      for (int i = 0; i < attributes.getLength(); ++i)
        itsRegularAttributes.insert(attributes[i]);
    }

    {
      const auto& attributes = itsConfig.lookup("presentation_attributes");
      if (!attributes.isArray())
      {
        throw Fmi::Exception::Trace(BCP, "Configuration error!")
            .addParameter("Configuration file", configfile)
            .addDetail("Configured value of 'presentation_attributes' must be an array");
      }

      for (int i = 0; i < attributes.getLength(); ++i)
        itsPresentationAttributes.insert(attributes[i]);
    }
  }

  catch (...)
  {
    Spine::Exceptions::handle("WMS plugin");
  }
}

// ----------------------------------------------------------------------
/*
 * Accessors
 */
// ----------------------------------------------------------------------

const std::string& Config::defaultUrl() const
{
  return itsDefaultUrl;
}
const std::string& Config::defaultModel() const
{
  return itsDefaultModel;
}
const std::string& Config::defaultLanguage() const
{
  return itsDefaultLanguage;
}
const std::set<std::string>& Config::languages() const
{
  return itsLanguages;
}
const std::string& Config::defaultTemplate() const
{
  return itsDefaultTemplate;
}
const std::string& Config::defaultCustomer() const
{
  return itsDefaultCustomer;
}
const std::string& Config::primaryForecastSource() const
{
  return itsPrimaryForecastSource;
}
const std::string& Config::templateDirectory() const
{
  return itsTemplateDirectory;
}
const std::string& Config::rootDirectory(bool theWmsFlag) const
{
  if (theWmsFlag)
    return itsWmsRootDirectory;
  return itsRootDirectory;
}
const std::string& Config::filesystemCacheDirectory() const
{
  return itsFilesystemCacheDirectory;
}
const std::set<std::string>& Config::regularAttributes() const
{
  return itsRegularAttributes;
}
const std::set<std::string>& Config::presentationAttributes() const
{
  return itsPresentationAttributes;
}

bool Config::isValidAttribute(const std::string& theName) const
{
  try
  {
    return (itsRegularAttributes.find(theName) != itsRegularAttributes.end() ||
            itsPresentationAttributes.find(theName) != itsPresentationAttributes.end());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string Config::defaultTemplate(const std::string& theType) const
{
  auto pos = itsDefaultTemplates.find(theType);
  if (pos != itsDefaultTemplates.end())
    return pos->second;

  pos = itsDefaultTemplates.find("default");
  if (pos != itsDefaultTemplates.end())
    return pos->second;

  // Backward compatibility in case the settings are missing
  return "svg";
}

double Config::defaultPrecision(const std::string& theType, const std::string& theName) const
{
  auto it = itsDefaultPrecisions.find(theType);
  if (it != itsDefaultPrecisions.end())
    return it->second;

  it = itsDefaultPrecisions.find(theName);
  if (it != itsDefaultPrecisions.end())
    return it->second;

  it = itsDefaultPrecisions.find("default");
  if (it != itsDefaultPrecisions.end())
    return it->second;

  // default precision is one decimal
  return 1.0;
}

Config::UnitConversion Config::unitConversion(const std::string& theUnitConversion) const
{
  auto pos = itsUnitConversionMap.find(theUnitConversion);
  if (pos == itsUnitConversionMap.end())
    throw Fmi::Exception(BCP, "Unknown unit conversion '" + theUnitConversion + "'");
  return pos->second;
}

const std::string& Config::wmsUrl() const
{
  return itsWmsUrl;
}
unsigned long long Config::maxMemoryCacheSize() const
{
  return itsMaxMemoryCacheSize;
}
unsigned long long Config::maxFilesystemCacheSize() const
{
  return itsMaxFilesystemCacheSize;
}

unsigned int Config::styleSheetCacheSize() const
{
  return itsStyleSheetCacheSize;
}

bool Config::quiet() const
{
  return itsQuiet;
}

unsigned int Config::maxImageSize() const
{
  return itsMaxImageSize;
}

unsigned int Config::maxWMSLayers() const
{
  return itsMaxWMSLayers;
}

unsigned Config::maxHeatmapPoints() const
{
  return itsMaxHeatmapPoints;
}

#ifndef WITHOUT_AUHTENTICATION
bool Config::authenticate() const
{
  return itsAuthenticate;
}
#endif

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
