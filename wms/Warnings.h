// ======================================================================
/*!
 * \brief Data structure errors which are not necessarily fatal during development
 */
// ======================================================================

#pragma once

#include <list>
#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct Warnings
{
  std::map<std::string, int> qid_counts;  // use count of qids
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
