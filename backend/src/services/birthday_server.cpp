#include "services/birthday_server.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "utils/file.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

void addCorsHeaders(httplib::Response& res) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.set_header("Access-Control-Allow-Headers", "Content-Type");
  res.set_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
}

}  // namespace

BirthdayServer::BirthdayServer(
    AppConfig config,
    std::vector<std::unique_ptr<Notifier>> notifiers)
    : config_(std::move(config)),
      notifiers_(std::move(notifiers)),
      icalFeedService_(config_.icalUrl),
      vcardFeedService_(config_.pendingDir),
  vcardWorkflow_(notifiers_) {}

int BirthdayServer::run() {
  fs::create_directories(config_.pendingDir);

  configureRoutes();

  if (fs::exists(config_.publicDir) && fs::is_directory(config_.publicDir)) {
    server_.set_mount_point("/", config_.publicDir.c_str());
  } else {
    std::cerr << "Warning: PUBLIC_DIR does not exist: " << config_.publicDir
              << std::endl;
  }

  server_.set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
    if (res.status != 404 || req.path.rfind("/api", 0) == 0) {
      return;
    }

    const fs::path indexPath = fs::path(config_.publicDir) / "index.html";
    auto indexHtml = readTextFile(indexPath);
    if (!indexHtml.has_value()) {
      return;
    }

    res.status = 200;
    res.set_content(indexHtml.value(), "text/html");
  });

  std::cout << "Birthday calendar server listening on 0.0.0.0:" << config_.port
            << std::endl;
  server_.listen("0.0.0.0", config_.port);
  return 0;
}

void BirthdayServer::configureRoutes() {
  server_.set_payload_max_length(65536);  // 64 KiB — protects against oversized POST bodies

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

  const IcalFeedResult feed = icalFeedService_.fetchBirthdays();
  if (!feed.ok) {
    res.status = feed.statusCode;
    res.set_content(
        json({{"error", feed.error}}).dump(),
        "application/json");
    return;
  }

  std::set<std::string> monthDays;
  json payload = json::array();

  for (const auto& event : feed.birthdays) {
    monthDays.insert(event.monthDay);
    payload.push_back({
        {"name", event.name},
        {"date", event.date},
        {"monthDay", event.monthDay},
    });
  }

  vcardFeedService_.cleanupImportedVcards(monthDays);
  vcardFeedService_.appendPendingBirthdays(monthDays, payload);

  res.set_content(payload.dump(), "application/json");
}

void BirthdayServer::handleCreateVcard(const httplib::Request& req,
                                       httplib::Response& res) {
  addCorsHeaders(res);
  const VcardSubmitResult result = vcardWorkflow_.submit(req.body);
  res.status = result.statusCode;
  res.set_content(result.body, "application/json");
}
