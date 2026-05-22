#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

std::optional<std::string> extractVcardBirthday(const std::string& vcardContent);
std::string buildVcard(const nlohmann::json& payload);
