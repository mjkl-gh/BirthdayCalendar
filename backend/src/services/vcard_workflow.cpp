#include "services/vcard_workflow.h"

#include <chrono>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "services/vcard.h"
#include "utils/file.h"
#include "utils/text.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

VcardWorkflow::VcardWorkflow(std::filesystem::path pendingDir,
                             std::vector<std::unique_ptr<Notifier>>& notifiers)
    : pendingDir_(std::move(pendingDir)), notifiers_(notifiers) {}

VcardSubmitResult VcardWorkflow::submit(const std::string& requestBody) const {
  json payload;
  try {
    payload = json::parse(requestBody);
  } catch (...) {
    return {
        .statusCode = 400,
        .body = R"({"error":"Invalid JSON"})",
    };
  }

  const std::string firstName = trim(payload.value("firstName", ""));
  const std::string lastName = trim(payload.value("lastName", ""));
  const std::string email = trim(payload.value("email", ""));
  const std::string birthday = trim(payload.value("birthday", ""));

  static const std::regex datePattern(R"(^\d{4}-\d{2}-\d{2}$)");
  if (firstName.empty() || lastName.empty() || email.empty() ||
      !std::regex_match(birthday, datePattern)) {
    return {
        .statusCode = 400,
        .body = R"({"error":"Missing or invalid form fields"})",
    };
  }

  const auto now = std::chrono::system_clock::now();
  const auto epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
          .count();
  std::string fileName = sanitizeFileName(firstName + "-" + lastName) + "-" +
                         std::to_string(epoch) + ".vcf";
  fs::path vcfPath = fs::path(pendingDir_) / fileName;

  const std::string vcard = buildVcard(payload);
  if (!writeTextFile(vcfPath, vcard)) {
    return {
        .statusCode = 500,
        .body = R"({"error":"Failed to persist vCard"})",
    };
  }

  std::stringstream message;
  message << "A new birthday request was submitted.\n";
  message << "Name: " << firstName << ' ' << lastName << "\n";
  message << "Email: " << email << "\n";
  message << "Birthday: " << birthday << "\n";

  bool anySent = false;
  for (const auto& notifier : notifiers_) {
    if (notifier->sendVcard("Birthday vCard submission", message.str(), vcfPath)) {
      anySent = true;
    }
  }

  if (!anySent) {
    return {
        .statusCode = 502,
        .body = R"({"error":"vCard stored but email delivery failed"})",
    };
  }

  return {
      .statusCode = 201,
      .body = R"({"ok":true,"status":"submitted"})",
  };
}
