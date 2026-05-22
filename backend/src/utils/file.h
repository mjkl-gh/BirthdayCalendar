#pragma once

#include <filesystem>
#include <optional>
#include <string>

std::optional<std::string> readTextFile(const std::filesystem::path& path);
bool writeTextFile(const std::filesystem::path& path, const std::string& content);
