#include "services/birthday_server.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "utils/file.h"
#include "services/ical.h"
#include "utils/text.h"
#include "services/vcard.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

void addCorsHeaders(httplib::Response& res) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.set_header("Access-Control-Allow-Headers", "Content-Type");
  res.set_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
}

}  // namespace

BirthdayServer::BirthdayServer(AppConfig config, std::unique_ptr<Notifier> sender)
    : config_(std::move(config)), sender_(std::move(sender)) {}

int BirthdayServer::run() {
  fs::create_directories(config_.pendingDir);

  configureRoutes();

  std::cout << "Birthday calendar server listening on 0.0.0.0:" << config_.port
            << std::endl;
  server_.listen("0.0.0.0", config_.port);
  return 0;
}

void BirthdayServer::configureRoutes() {
  server_.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
    addCorsHeaders(res);
    return httplib::Server::HandlerResponse::Unhandled;
  });

  server_.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
    addCorsHeaders(res);
    res.status = 204;
  });

  server_.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
    addCorsHeaders(res);
    res.set_content(R"({"ok":true})", "application/json");
  });

  server_.Get("/api/birthdays", [this](const httplib::Request& req, httplib::Response& res) {
    handleGetBirthdays(req, res);
  });

  server_.Post("/api/vcards", [this](const httplib::Request& req, httplib::Response& res) {
    handleCreateVcard(req, res);
  });
}

void BirthdayServer::handleGetBirthdays(const httplib::Request&, httplib::Response& res) {
  addCorsHeaders(res);

  if (config_.icalUrl.empty()) {
    res.status = 500;
    res.set_content(R"({"error":"ICAL_URL is not configured"})", "application/json");
    return;
  }

  auto ics = fetchRemoteText(config_.icalUrl);
  if (!ics.has_value()) {
    res.status = 502;
    res.set_content(R"({"error":"Could not fetch iCal feed"})", "application/json");
    return;
  }

  std::vector<BirthdayEvent> events = parseIcsBirthdays(ics.value());
  std::set<std::string> monthDays;
  json payload = json::array();

  for (const auto& event : events) {
    monthDays.insert(event.monthDay);
    payload.push_back({
        {"name", event.name},
        {"date", event.date},
        {"monthDay", event.monthDay},
    });
  }

  cleanupImportedVcards(monthDays);

  res.set_content(payload.dump(), "application/json");
}

void BirthdayServer::handleCreateVcard(const httplib::Request& req,
                                       httplib::Response& res) {
  addCorsHeaders(res);

  json payload;
  try {
    payload = json::parse(req.body);
  } catch (...) {
    res.status = 400;
    res.set_content(R"({"error":"Invalid JSON"})", "application/json");
    return;
  }

  const std::string firstName = trim(payload.value("firstName", ""));
  const std::string lastName = trim(payload.value("lastName", ""));
  const std::string email = trim(payload.value("email", ""));
  const std::string birthday = trim(payload.value("birthday", ""));

  static const std::regex datePattern(R"(^\d{4}-\d{2}-\d{2}$)");
  if (firstName.empty() || lastName.empty() || email.empty() ||
      !std::regex_match(birthday, datePattern)) {
    res.status = 400;
    res.set_content(R"({"error":"Missing or invalid form fields"})",
                    "application/json");
    return;
  }

  const auto now = std::chrono::system_clock::now();
  const auto epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
          .count();
  std::string fileName = sanitizeFileName(firstName + "-" + lastName) + "-" +
                         std::to_string(epoch) + ".vcf";
  fs::path vcfPath = fs::path(config_.pendingDir) / fileName;

  const std::string vcard = buildVcard(payload);
  if (!writeTextFile(vcfPath, vcard)) {
    res.status = 500;
    res.set_content(R"({"error":"Failed to persist vCard"})", "application/json");
    return;
  }

  std::stringstream message;
  message << "A new birthday request was submitted.\n";
  message << "Name: " << firstName << ' ' << lastName << "\n";
  message << "Email: " << email << "\n";
  message << "Birthday: " << birthday << "\n";

  bool sent = sender_->sendVcard("Birthday vCard submission", message.str(), vcfPath);
  if (!sent) {
    res.status = 502;
    res.set_content(R"({"error":"vCard stored but email delivery failed"})",
                    "application/json");
    return;
  }

  res.status = 201;
  res.set_content(R"({"ok":true,"status":"submitted"})", "application/json");
}

void BirthdayServer::cleanupImportedVcards(
    const std::set<std::string>& currentMonthDays) {
  if (!fs::exists(config_.pendingDir)) {
    return;
  }

  for (const auto& entry : fs::directory_iterator(config_.pendingDir)) {
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
