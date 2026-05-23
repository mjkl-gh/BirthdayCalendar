#include "services/vcard_workflow.h"

#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "utils/text.h"

VcardWorkflow::VcardWorkflow(std::vector<std::unique_ptr<Notifier>>& notifiers)
  : notifiers_(notifiers) {}

std::pair<int, json> VcardWorkflow::submit(const std::string& requestBody) const {
  json payload;
  try {
    payload = json::parse(requestBody);
  } catch (...) {
    return {400, json{{"error", "Invalid JSON"}}};
  }

  const std::string firstName = trim(payload.value("firstName", ""));
  const std::string lastName = trim(payload.value("lastName", ""));
  const std::string email = trim(payload.value("email", ""));
  const std::string birthday = trim(payload.value("birthday", ""));
  const std::string notes = trim(payload.value("notes", ""));

  static const std::regex datePattern(R"(^\d{4}-\d{2}-\d{2}$)");
  static const std::regex emailPattern(R"([^@\s]+@[^@\s]+\.[^@\s]+)");

  if (firstName.empty() || firstName.size() > 100 ||
      lastName.empty() || lastName.size() > 100 ||
      email.empty() || email.size() > 254 ||
      !std::regex_match(email, emailPattern) ||
      !std::regex_match(birthday, datePattern) ||
      notes.size() > 1000) {
    return {400, json{{"error", "Missing or invalid form fields"}}};
  }

  const Vcard submission{
      .firstName = firstName,
      .lastName = lastName,
      .email = email,
      .birthday = birthday,
      .notes = notes,
  };

  std::vector<std::string> errors;
  int sentCount = 0;
  for (const auto& notifier : notifiers_) {
    try {
      notifier->sendVcard(submission);
      ++sentCount;
    } catch (const std::exception& e) {
      std::cerr << "[Notifier] sendVcard failed: " << e.what() << '\n';
      errors.push_back(e.what());
    }
  }

  if (sentCount == 0 && !notifiers_.empty()) {
    json response = {
      {"error", "All notification channels failed"},
      {"notifierErrors", errors},
    };
    return {502, response};
  }

  json response = {
    {"ok", true},
    {"status", "submitted"},
  };
  if (!errors.empty()) {
    response["warning"] = "Submitted, but some notification channels failed";
    response["notifierErrors"] = errors;
  }

  return {201, response};
}
