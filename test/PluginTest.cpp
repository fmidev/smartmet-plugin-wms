#include "Plugin.h"

#include <smartmet/spine/HTTP.h>
#include <smartmet/spine/Options.h>
#include <smartmet/spine/Reactor.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

namespace SmartMet
{
namespace MyTest
{
typedef boost::function<void(SmartMet::Spine::Reactor& reactor)> PreludeFunction;

}  // namespace MyTest
}  // namespace SmartMet

// ----------------------------------------------------------------------
// IMPLEMENTATION DETAILS
// ----------------------------------------------------------------------

namespace
{
// ---------------------------------------------------------------------
namespace ba = boost::algorithm;

std::vector<std::string> read_file(const std::string& fn)
{
  std::ifstream input(fn.c_str());
  std::vector<std::string> result;
  std::string line;
  while (std::getline(input, line))
  {
    result.push_back(ba::trim_right_copy_if(line, ba::is_any_of("\r\n")));
  }
  return result;
}

// ----------------------------------------------------------------------

namespace fs = boost::filesystem;

// ----------------------------------------------------------------------

std::string get_file_contents(const boost::filesystem::path& filename)
{
  std::string content;
  std::ifstream in(filename.c_str());
  if (in)
    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return content;
}

// ----------------------------------------------------------------------

void put_file_contents(const boost::filesystem::path& filename, const std::string& data)
{
  std::ofstream out(filename.c_str());
  if (out)
  {
    out << data;
    out.close();
  }
  else
    std::cerr << "Failed to open " << filename << " for writing" << std::endl;
}

// ----------------------------------------------------------------------

std::string get_full_response(SmartMet::Spine::HTTP::Response& response)
{
  std::string result = response.getContent();

  if (result.empty())
    return result;

  if (response.hasStreamContent())
  {
    while (true)
    {
      std::string tmp = response.getContent();
      if (tmp.empty())
        break;
      result += tmp;
    }
  }

  return result;
}
}

namespace SmartMet
{
namespace MyTest
{
void test(SmartMet::Spine::Options& options, PreludeFunction prelude)
{
  options.parseConfig();
  SmartMet::Spine::Reactor reactor(options);
  prelude(reactor);

  std::cout << "OK" << std::endl;

  std::string command;
  while (getline(std::cin, command))
  {
    if (command == "quit")
      break;

    using std::cout;
    using std::endl;
    using std::string;
    using boost::filesystem::path;

    path inputfile("input");
    inputfile /= command;

    bool ok = true;

    string input = get_file_contents(inputfile);

    // emacs keeps messing up the newlines, easier to make sure
    // the ending is correct this way, but do NOT touch POST queries

    if (boost::algorithm::ends_with(inputfile.string(), ".get"))
    {
      boost::algorithm::trim(input);
      input += "\r\n\r\n";
    }

    auto query = SmartMet::Spine::HTTP::parseRequest(input);

    path resultfile = path("failures") / command;
    boost::filesystem::create_directories(resultfile.parent_path());

    if (query.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
    {
      try
      {
        SmartMet::Spine::HTTP::Response response;

        auto view = reactor.getHandlerView(*query.second);
        if (!view)
        {
          ok = false;
          cout << "FAILED TO HANDLE REQUEST STRING" << endl;
        }
        else
        {
          view->handle(reactor, *query.second, response);

          string result = get_full_response(response);

          if (ok)
            put_file_contents(resultfile, result);
        }
      }
      catch (std::exception& e)
      {
        ok = false;
        cout << "EXCEPTION: " << e.what() << endl;
      }
      catch (...)
      {
        ok = false;
        cout << "UNKNOWN EXCEPTION" << endl;
      }
    }
    else if (query.first == SmartMet::Spine::HTTP::ParsingStatus::FAILED)
    {
      ok = false;
      cout << "FAILED TO PARSE REQUEST STRING" << endl;
    }
    else
    {
      ok = false;
      cout << "PARSED REQUEST ONLY PARTIALLY" << endl;
    }

    if (!ok)
      put_file_contents(resultfile, "");

    cout << "DONE" << endl;
  }
}

}  // namespace MyTest
}  // namespace SmartMet

void prelude(SmartMet::Spine::Reactor& reactor)
{
  auto handlers = reactor.getURIMap();
  while (handlers.find("/dali") == handlers.end())
  {
    sleep(1);
    handlers = reactor.getURIMap();
  }

  std::cout << std::endl
            << "Testing dali plugin" << std::endl
            << "===================" << std::endl;
}

int main()
{
  SmartMet::Spine::Options options;
  options.quiet = true;
  options.defaultlogging = false;
  options.configfile = "cnf/reactor.conf";

  SmartMet::MyTest::test(options, prelude);

  return 0;
}
