#include "utils/file.h"

#include <filesystem>
#include <fstream>
#include <sstream>

std::optional<std::string> readTextFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return std::nullopt;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

bool writeTextFile(const std::filesystem::path& path,
                   const std::string& content) {
  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }
  file << content;
  return file.good();
}
