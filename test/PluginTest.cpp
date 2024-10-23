#include "Plugin.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StaticCleanup.h>
#include <smartmet/spine/HTTP.h>
#include <smartmet/spine/Options.h>
#include <smartmet/spine/Reactor.h>
#include <algorithm>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

static const int testtimeout = 900;  // Seconds a single test is allowed to run

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

namespace fs = std::filesystem;

// ----------------------------------------------------------------------

std::string get_file_contents(const std::filesystem::path& filename)
{
  std::string content;
  std::ifstream in(filename.c_str());
  if (in)
    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return content;
}

// ----------------------------------------------------------------------

void put_file_contents(const std::filesystem::path& filename, const std::string& data)
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
}  // namespace

namespace SmartMet
{
namespace MyTest
{
void test(SmartMet::Spine::Options& options, PreludeFunction prelude)
{
  try
  {
    Fmi::StaticCleanup::AtExit cleanup;
    options.parseConfig();
    auto* reactor = new SmartMet::Spine::Reactor(options);
    reactor->init();
    if (reactor->isShuttingDown())
    {
      throw Fmi::Exception::Trace(BCP,
                                  "SmartMet::Spine::Reactor shutdown detected while init phase.");
    }
    prelude(*reactor);

    std::cout << "OK" << std::endl;

    std::string command;
    while (getline(std::cin, command))
    {
      if (command == "quit")
        break;

      using std::cout;
      using std::endl;
      using std::string;
      using std::filesystem::path;

      path inputfile("input");
      inputfile /= command;

      bool ok = true;

      // Set timeout for this test
      alarm(testtimeout);

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
      std::filesystem::create_directories(resultfile.parent_path());

      if (query.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
      {
        try
        {
          SmartMet::Spine::HTTP::Response response;

          auto view = reactor->getHandlerView(*query.second);
          if (!view)
          {
            ok = false;
            cout << "FAILED TO HANDLE REQUEST STRING" << endl;
          }
          else
          {
            view->handle(*reactor, *query.second, response);

            string result = get_full_response(response);

            if (ok)
              put_file_contents(resultfile, result);
          }
        }
        catch (const std::exception& e)
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

      // Remove timeout for this test
      alarm(0);

      if (!ok)
        put_file_contents(resultfile, "");

      cout << "DONE" << endl;
    }
    reactor->shutdown();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to run the tests");
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

void alarmhandler(int signal)
{
  std::cerr << std::endl
            << "Timeout: Test terminated because " << testtimeout
            << " seconds have passed. Test stuck in infinite loop?" << std::endl;
  exit(1);
}

int main(int argc, char* argv[])
try
{
  SmartMet::Spine::Options options;
  options.quiet = true;
  options.defaultlogging = false;
  options.configfile = "cnf/reactor.conf";

  if (!options.parse(argc, argv))
    exit(1);

  signal(SIGALRM, alarmhandler);

  // Set stdout to line buffered and stderr to unbuffered
  setvbuf(stdout, nullptr, _IOLBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  SmartMet::MyTest::test(options, prelude);

  return 0;
}
catch (...)
{
  Fmi::Exception ex(BCP, "Failed to run the tests", nullptr);
  ex.printError();
  return 1;
}
