#include "StyleSheet.h"
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Add CSS contents
 *
 * For simplicity we look only for CSS class settings as in
 *
 *  .Temperature { fill: none; stroke: black; }
 *
 * The respective regexes we use are:
 *
 *  selector openblock property equals value nextproperty property .... value closeblock
 */
// ----------------------------------------------------------------------

void StyleSheet::add(const std::string& theCSS)
{
  const boost::regex REcomments{R"(/\*[^\*]*\*+([^/\*][^\*]*\*+)*/)"};

  const boost::regex REselector{R"(^([^\{]+))"};
  const boost::regex REopenblock{R"(^\{\s*)"};
  const boost::regex REcloseblock{R"(^\})"};

  const boost::regex REproperty{
      R"(^(\*?[-#/\*\w]+(\[[0-9a-z_-]+\])?)\s*)"};
  const boost::regex REequals{R"(^:\s*)"};
  const boost::regex REvalue{
      R"(^((?:'(?:\\'|.)*?'|\"(?:\\\"|.)*?\"|\([^\)]*?\)|[^\};])+))"};
  const boost::regex REnextproperty{R"(^[;\s]*)"};

  // Remove comments
  auto css = boost::regex_replace(theCSS, REcomments, "");

  // Parse sequentially
  auto pos = css.cbegin();
  auto end = css.cend();
  boost::smatch match;

  while (true)
  {
    if (!boost::regex_search(pos, end, match, REselector))
      break;
    std::string selector = match[0];
    pos += selector.length();
    boost::algorithm::trim(selector);

    if (!boost::regex_search(pos, end, match, REopenblock))
      break;

    pos += match[0].length();

    while (true)
    {
      if (!boost::regex_search(pos, end, match, REproperty))
        break;
      std::string property = match[0];
      pos += property.length();
      boost::algorithm::trim(property);

      if (!boost::regex_search(pos, end, match, REequals))
        break;

      pos += match[0].length();

      if (!boost::regex_search(pos, end, match, REvalue))
        break;

      std::string value = match[0];
      pos += value.length();
      boost::algorithm::trim(value);

      itsStyleSheet[selector][property] = value;

      if (boost::regex_search(pos, end, match, REnextproperty))
        pos += match[0].length();
    }

    if (!boost::regex_search(pos, end, match, REcloseblock))
      break;
    pos += match[0].length();
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the CSS settings for the given class
 */
// ----------------------------------------------------------------------

const StyleSheet::Declarations& StyleSheet::declarations(const std::string& theSelector) const
{
  // static dummy so that we can return a const reference
  static Declarations dummy{};

  const auto iter = itsStyleSheet.find(theSelector);
  if (iter == itsStyleSheet.end())
    return dummy;

  return iter->second;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
