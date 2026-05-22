#include "utils/text.h"

#include <algorithm>
#include <cctype>
#include <regex>

std::string trim(const std::string& value) {
  const std::string ws = " \t\r\n";
  const size_t start = value.find_first_not_of(ws);
  if (start == std::string::npos) {
    return "";
  }
  const size_t end = value.find_last_not_of(ws);
  return value.substr(start, end - start + 1);
}

std::string sanitizeFileName(const std::string& raw) {
  std::string out;
  out.reserve(raw.size());
  for (char c : raw) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
      out.push_back(c);
    }
  }
  if (out.empty()) {
    out = "submission";
  }
  return out;
}

std::string normalizeDate(const std::string& raw) {
  std::string value = trim(raw);
  const size_t colonPos = value.find(':');
  if (colonPos != std::string::npos) {
    value = value.substr(colonPos + 1);
  }

  if (value.size() >= 8 && std::isdigit(value[0]) != 0) {
    std::string y = value.substr(0, 4);
    std::string m = value.substr(4, 2);
    std::string d = value.substr(6, 2);
    if (std::all_of(y.begin(), y.end(), ::isdigit) &&
        std::all_of(m.begin(), m.end(), ::isdigit) &&
        std::all_of(d.begin(), d.end(), ::isdigit)) {
      return y + "-" + m + "-" + d;
    }
  }

  static const std::regex isoPattern(R"(^\d{4}-\d{2}-\d{2}$)");
  if (std::regex_match(value, isoPattern)) {
    return value;
  }

  return "";
}

std::string toMonthDay(const std::string& isoDate) {
  if (isoDate.size() != 10) {
    return "";
  }
  return isoDate.substr(5, 5);
}
