// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <stdexcept>

using std::string;

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
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
    itsConfig.readFile(configfile.c_str());

    // required parameters
    std::string root = itsConfig.lookup("root");
    std::string wmsroot = itsConfig.lookup("wms.root");
    itsRootDirectory = root;
    itsWmsRootDirectory = wmsroot;

    // optional parameters
    itsConfig.lookupValue("url", itsDefaultUrl);
    itsConfig.lookupValue("model", itsDefaultModel);
    itsConfig.lookupValue("language", itsDefaultLanguage);

    itsConfig.lookupValue("template", itsDefaultTemplate);
    itsConfig.lookupValue("templatedir", itsTemplateDirectory);
    itsConfig.lookupValue("customer", itsDefaultCustomer);

    itsConfig.lookupValue("cache.memory_bytes", itsMaxMemoryCacheSize);
    itsConfig.lookupValue("cache.filesystem_bytes", itsMaxFilesystemCacheSize);
    itsConfig.lookupValue("cache.directory", itsFilesystemCacheDirectory);

    itsConfig.lookupValue("heatmap.max_points", itsMaxHeatmapPoints);

    itsConfig.lookupValue("wms.url", itsWmsUrl);
    itsConfig.lookupValue("wms.quiet", itsQuiet);

#ifndef WITHOUT_AUHTENTICATION
    itsConfig.lookupValue("authenticate", itsAuthenticate);
#endif

#ifndef WITHOUT_OBSERVATION
    itsConfig.lookupValue("observation_disabled", itsObsEngineDisabled);
#endif

    // Default templates for various types

    {
      const auto& templates = itsConfig.lookup("templates");
      if (!templates.isGroup())
      {
        throw Spine::Exception::Trace(BCP, "Configuration error!")
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
        throw Spine::Exception::Trace(BCP, "Configuration error!")
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
        throw Spine::Exception::Trace(BCP, "Configuration error!")
            .addParameter("Configuration file", configfile)
            .addDetail("Configured value of 'unit_conversions' must be an array");
      }
      for (int i = 0; i < conversions.getLength(); ++i)
      {
        const auto& conversion = conversions[i];
        if (!conversion.isGroup())
        {
          throw Spine::Exception::Trace(BCP, "Configuration error!")
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
        throw Spine::Exception::Trace(BCP, "Configuration error!")
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
        throw Spine::Exception::Trace(BCP, "Configuration error!")
            .addParameter("Configuration file", configfile)
            .addDetail("Configured value of 'presentation_attributes' must be an array");
      }

      for (int i = 0; i < attributes.getLength(); ++i)
        itsPresentationAttributes.insert(attributes[i]);
    }
  }

  catch (const libconfig::SettingNotFoundException& e)
  {
    throw Spine::Exception(BCP, "Setting not found").addParameter("Setting path", e.getPath());
  }
  catch (const libconfig::ParseException& e)
  {
    throw Spine::Exception::Trace(BCP, "Configuration error!")
        .addParameter("Configuration file", configfile)
        .addParameter("Line", Fmi::to_string(e.getLine()));
  }
  catch (const libconfig::ConfigException&)
  {
    throw Spine::Exception::Trace(BCP, "Configuration error!")
        .addParameter("Configuration file", configfile);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Configuration error!")
        .addParameter("Configuration file", configfile);
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
const std::string& Config::defaultTemplate() const
{
  return itsDefaultTemplate;
}
const std::string& Config::defaultCustomer() const
{
  return itsDefaultCustomer;
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

double Config::defaultPrecision(const std::string& theName) const
{
  auto it = itsDefaultPrecisions.find(theName);
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
    throw Spine::Exception(BCP, "Unknown unit conversion '" + theUnitConversion + "'");
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
bool Config::quiet() const
{
  return itsQuiet;
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
