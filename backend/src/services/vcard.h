#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "models.h"

std::optional<std::string> extractVcardBirthday(const std::string& vcardContent);
std::string buildVcard(const Vcard& submission);
