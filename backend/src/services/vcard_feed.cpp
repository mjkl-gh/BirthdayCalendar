#include "services/vcard_feed.h"

#include <filesystem>
#include <optional>
#include <set>
#include <sstream>
#include <string>

#include "services/vcard.h"
#include "utils/file.h"
#include "utils/text.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::optional<std::string> extractVcardName(const std::string& vcardContent) {
  std::istringstream stream(vcardContent);
  std::string line;

  while (std::getline(stream, line)) {
    line = trim(line);
    if (line.rfind("FN:", 0) == 0) {
      std::string value = trim(line.substr(3));
      if (!value.empty()) {
        return value;
      }
    }
  }

  return std::nullopt;
}

}  // namespace

VcardFeedService::VcardFeedService(std::filesystem::path pendingDir)
    : pendingDir_(std::move(pendingDir)) {}

void VcardFeedService::cleanupImportedVcards(
    const std::set<std::string>& currentMonthDays) const {
  if (!fs::exists(pendingDir_)) {
    return;
  }

  for (const auto& entry : fs::directory_iterator(pendingDir_)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".vcf") {
      continue;
    }

    std::optional<std::string> content = readTextFile(entry.path());
    if (!content.has_value()) {
      continue;
    }

    std::optional<std::string> birthday = extractVcardBirthday(content.value());
    if (!birthday.has_value()) {
      continue;
    }

    std::string monthDay = toMonthDay(birthday.value());
    if (!monthDay.empty() && currentMonthDays.contains(monthDay)) {
      std::error_code ec;
      fs::remove(entry.path(), ec);
    }
  }
}

void VcardFeedService::appendPendingBirthdays(std::set<std::string>& monthDays,
                                              json& payload) const {
  if (!fs::exists(pendingDir_)) {
    return;
  }

  for (const auto& entry : fs::directory_iterator(pendingDir_)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".vcf") {
      continue;
    }

    std::optional<std::string> content = readTextFile(entry.path());
    if (!content.has_value()) {
      continue;
    }

    std::optional<std::string> birthday = extractVcardBirthday(content.value());
    if (!birthday.has_value()) {
      continue;
    }

    const std::string monthDay = toMonthDay(birthday.value());
    if (monthDay.empty() || monthDays.contains(monthDay)) {
      continue;
    }

    const std::string name =
        extractVcardName(content.value()).value_or("Pending birthday");

    monthDays.insert(monthDay);
    payload.push_back({
        {"name", name},
        {"date", birthday.value()},
        {"monthDay", monthDay},
        {"pending", true},
    });
  }
}
