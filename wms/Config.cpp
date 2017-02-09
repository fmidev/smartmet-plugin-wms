// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include <spine/Exception.h>
#include <macgyver/String.h>
#include <stdexcept>

using namespace std;

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
    : itsConfig(),
      itsDefaultUrl("/dali"),
      itsDefaultTemplate("svg"),
      itsDefaultCustomer("fmi"),
      itsDefaultModel("pal_skandinavia"),
      itsDefaultLanguage("en"),
      itsTemplateDirectory("/etc/smartmet/plugins/dali/templates"),
      itsRootDirectory(""),
      itsFilesystemCacheDirectory("/var/smartmet/imagecache"),
      itsMaxMemoryCacheSize(104857600)  // 100 MB
      ,
      itsMaxFilesystemCacheSize(209715200)  // 200 MB
      ,
      itsWmsUrl("/wms"),
      itsWmsRootDirectory(),
      itsRegularAttributes(),
      itsPresentationAttributes(),
#ifndef WITHOUT_AUHTENTICATION
      itsQuiet(false),
      itsAuthenticate(true)
#else
      itsQuiet(false)
#endif
{
  try
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

      itsConfig.lookupValue("wms.url", itsWmsUrl);
      itsConfig.lookupValue("wms.versions", itsWmsVersions);
      itsConfig.lookupValue("wms.mapformats", itsWmsMapFormats);
      itsConfig.lookupValue("wms.quiet", itsQuiet);

#ifndef WITHOUT_AUHTENTICATION
      itsConfig.lookupValue("authenticate", itsAuthenticate);
#endif

      // Store array of SVG attribute names into a set for looking up valid names
      {
        const auto& attributes = itsConfig.lookup("regular_attributes");
        if (!attributes.isArray())
        {
          SmartMet::Spine::Exception exception(BCP, "Configuration error!", NULL);
          exception.addParameter("Configuration file", configfile);
          exception.addDetail("Configured value of 'regular_attributes' must be an array");
          throw exception;
        }
        for (int i = 0; i < attributes.getLength(); ++i)
          itsRegularAttributes.insert(attributes[i]);
      }

      {
        const auto& attributes = itsConfig.lookup("presentation_attributes");
        if (!attributes.isArray())
        {
          SmartMet::Spine::Exception exception(BCP, "Configuration error!", NULL);
          exception.addParameter("Configuration file", configfile);
          exception.addDetail("Configured value of 'presentation_attributes' must be an array");
          throw exception;
        }

        for (int i = 0; i < attributes.getLength(); ++i)
          itsPresentationAttributes.insert(attributes[i]);
      }
    }
    catch (libconfig::ParseException& e)
    {
      SmartMet::Spine::Exception exception(BCP, "Configuration error!", NULL);
      exception.addParameter("Configuration file", configfile);
      exception.addParameter("Line", Fmi::to_string(e.getLine()));
      throw exception;
    }
    catch (libconfig::ConfigException&)
    {
      SmartMet::Spine::Exception exception(BCP, "Configuration error!", NULL);
      exception.addParameter("Configuration file", configfile);
      throw exception;
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Configuration error!", NULL);
      exception.addParameter("Configuration file", configfile);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Configuration failed!", NULL);
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
  else
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

const std::string& Config::wmsUrl() const
{
  return itsWmsUrl;
}
const std::string& Config::wmsVersions() const
{
  return itsWmsVersions;
}
const std::string& Config::wmsMapFormats() const
{
  return itsWmsMapFormats;
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
