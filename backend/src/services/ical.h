#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../models.h"

std::optional<std::string> fetchRemoteText(const std::string& url);
std::vector<BirthdayEvent> parseIcsBirthdays(const std::string& icsText);
