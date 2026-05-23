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

bool startsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

}  // namespace

BirthdayServer::BirthdayServer(
    AppConfig config,
    std::vector<std::unique_ptr<Notifier>> notifiers)
    : config_(std::move(config)),
      notifiers_(std::move(notifiers)),
      icalFeedService_(config_.icalUrl),
      authService_(config_),
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

  server_.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
    addCorsHeaders(res);

    if (req.method == "OPTIONS") {
      return httplib::Server::HandlerResponse::Unhandled;
    }

    const bool isPublicApi =
        req.path == "/api/health" ||
        req.path == "/api/auth/qr" ||
        req.path == "/api/auth/exchange";
    const bool isAuthPage = req.path == "/auth";
    const bool isStaticAsset =
        startsWith(req.path, "/assets/") ||
        req.path == "/favicon.ico";

    const auto token = authService_.extractTokenFromRequest(req);
    const bool isAuthenticated =
      token.has_value() && authService_.validateSessionToken(token.value());

    if (startsWith(req.path, "/api")) {
      if (!isPublicApi && !isAuthenticated) {
        res.status = 401;
        res.set_content(R"({"error":"Authentication required"})", "application/json");
        return httplib::Server::HandlerResponse::Handled;
      }
      return httplib::Server::HandlerResponse::Unhandled;
    }

    if (isAuthPage || isStaticAsset) {
      if (isAuthPage && isAuthenticated) {
        res.set_redirect("/");
        return httplib::Server::HandlerResponse::Handled;
      }
      return httplib::Server::HandlerResponse::Unhandled;
    }

    if (!isAuthenticated) {
      res.set_header("Cache-Control", "no-store");
      res.set_redirect("/auth");
      return httplib::Server::HandlerResponse::Handled;
    }

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

  server_.Get("/api/auth/qr", [this](const httplib::Request& req, httplib::Response& res) {
    handleAuthQr(req, res);
  });

  server_.Post("/api/auth/exchange", [this](const httplib::Request& req, httplib::Response& res) {
    handleAuthExchange(req, res);
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
  std::set<std::string> monthDays;
  json payload = json::array();

  if (feed.ok) {
    for (const auto& event : feed.birthdays) {
      monthDays.insert(event.monthDay);
      payload.push_back({
          {"name", event.name},
          {"date", event.date},
          {"monthDay", event.monthDay},
      });
    }
  } else {
    // Keep API usable even when remote iCal is down/misconfigured.
    res.set_header("X-Calendar-Warning", feed.error);
  }

  vcardFeedService_.cleanupImportedVcards(monthDays);
  vcardFeedService_.appendPendingBirthdays(monthDays, payload);

  res.set_content(payload.dump(), "application/json");
}

void BirthdayServer::handleAuthQr(const httplib::Request& req,
                                  httplib::Response& res) {
  addCorsHeaders(res);

  if (!authService_.isLocalQrClient(req)) {
    res.status = 403;
    res.set_content(R"({"error":"QR token endpoint is only available from local networks"})", "application/json");
    return;
  }

  const JwtTokenInfo token = authService_.currentQrToken();
  res.set_header("Cache-Control", "no-store");
  res.set_content(
      json({
          {"token", token.token},
          {"expiresAt", token.expiresAtEpoch},
      }).dump(),
      "application/json");
}

void BirthdayServer::handleAuthExchange(const httplib::Request& req,
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

  const std::string qrToken = payload.value("token", "");
  if (qrToken.empty() || !authService_.validateQrToken(qrToken)) {
    res.status = 401;
    res.set_content(R"({"error":"Invalid token"})", "application/json");
    return;
  }

  const JwtTokenInfo session = authService_.issueSessionToken();
  const std::string forwardedProto = req.get_header_value("X-Forwarded-Proto");
  const bool secureCookie = forwardedProto == "https";
  res.set_header(
      "Set-Cookie",
      authService_.buildAuthCookie(session.token, secureCookie,
                                   config_.jwtSessionLifetimeSeconds));
  res.set_content(
      json({
          {"ok", true},
          {"sessionExpiresAt", session.expiresAtEpoch},
      }).dump(),
      "application/json");
}

void BirthdayServer::handleCreateVcard(const httplib::Request& req,
                                       httplib::Response& res) {
  addCorsHeaders(res);
  const VcardSubmitResult result = vcardWorkflow_.submit(req.body);
  res.status = result.statusCode;
  res.set_content(result.body, "application/json");
}
