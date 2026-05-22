#pragma once

#include <string>

std::string trim(const std::string& value);
std::string sanitizeFileName(const std::string& raw);
std::string normalizeDate(const std::string& raw);
std::string toMonthDay(const std::string& isoDate);
