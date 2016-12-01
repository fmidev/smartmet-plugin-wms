#pragma once

#include <dtl/dtl.hpp>

#include <thor/json.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>

std::vector<std::string> read_file(const std::string& fn)
{
  std::ifstream input(fn.c_str());
  std::vector<std::string> result;
  std::string line;
  while (std::getline(input, line))
  {
    result.push_back(
        boost::algorithm::trim_right_copy_if(line, boost::algorithm::is_any_of("\r\n")));
  }
  return result;
}

void show_diff(const std::string& src, const std::string& dest)
{
  const auto f1 = read_file(src);
  const auto f2 = read_file(dest);
  dtl::Diff<std::string> d(f1, f2);
  d.compose();
  d.composeUnifiedHunks();
  d.printUnifiedFormat(std::cout);
}

std::string get_file_contents(const boost::filesystem::path& filename)
{
  std::string content;
  std::ifstream in(filename.c_str());
  if (in) content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return content;
}

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

template <typename T>
void write_json(const T& ob, const std::string& filename)
{
  std::ofstream out(filename);
  if (!out) throw std::runtime_error("Failed to open '" + filename + "' for writing");
  out << Thor::Serialize::jsonExport(ob);
}

template <typename T>
std::string json_string(const T& ob)
{
  std::ostringstream out;
  out << Thor::Serialize::jsonExport(ob);
  return out.str();
}

template <typename T>
T read_json(const std::string& filename)
{
  std::ifstream in(filename);
  if (!in) throw std::runtime_error("Failed to open '" + filename + "' for reading");
  T ob;
  in >> Thor::Serialize::jsonImport(ob);
  return ob;
}
