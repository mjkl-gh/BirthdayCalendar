#include "services/birthday_server.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "utils/file.h"
#include <thread>

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

// Returns a sanitized representation of a URL suitable for logs.
// Removes userinfo and replaces the query string with "?[REDACTED]" when present.
std::string sanitizeUrlForLogging(const std::string& url) {
  if (url.empty()) return "";
  // Find scheme
  const auto schemePos = url.find("://");
  if (schemePos == std::string::npos) {
    // Not a full URL — redact any query component
    const auto q = url.find('?');
    if (q == std::string::npos) return url;
    return url.substr(0, q) + "?[REDACTED]";
  }

  const std::string scheme = url.substr(0, schemePos);
  std::string rest = url.substr(schemePos + 3);

  // Strip userinfo if present (user:pass@host)
  const auto atPos = rest.find('@');
  if (atPos != std::string::npos) {
    rest = rest.substr(atPos + 1);
  }

  // Separate path (including possible query) from host
  const auto slashPos = rest.find('/');
  std::string host = (slashPos == std::string::npos) ? rest : rest.substr(0, slashPos);
  std::string pathAndQuery = (slashPos == std::string::npos) ? std::string() : rest.substr(slashPos);

  // If there's a query string, redact it
  const auto qpos = pathAndQuery.find('?');
  if (qpos != std::string::npos) {
    pathAndQuery = pathAndQuery.substr(0, qpos) + "?[REDACTED]";
  }

  return scheme + "://" + host + pathAndQuery;
}

std::string percentEncodeQueryValue(const std::string& value) {
  static const char* kHex = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size() * 3);
  for (unsigned char c : value) {
    const bool isUnreserved =
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~';
    if (isUnreserved) {
      out.push_back(static_cast<char>(c));
      continue;
    }
    out.push_back('%');
    out.push_back(kHex[(c >> 4) & 0x0F]);
    out.push_back(kHex[c & 0x0F]);
  }
  return out;
}

std::string buildAuthUrl(const std::string& token, const AppConfig& config, const httplib::Request&) {
  std::string baseUrl = config.publicBaseUrl;
  if (baseUrl.back() == '/') {
    baseUrl.pop_back();
  }
  return baseUrl + "/auth?token=" + percentEncodeQueryValue(token);
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
  if (config_.authEnabled && config_.publicBaseUrl.empty()) {
    std::cerr << "Error: AUTH_ENABLED is true but PUBLIC_BASE_URL is not set. "
              << "Set PUBLIC_BASE_URL please.\n"
              << "Disable auth with AUTH_ENABLED=false. This should only be done"
              << " for development purposes.\n";
    return 1;
  }

  try {
    fs::create_directories(config_.pendingDir);
  } catch (const std::exception& ex) {
    std::cerr << "Error: failed to create pending directory '" << config_.pendingDir
              << "': " << ex.what() << std::endl;
    return 1;
  }

  // Startup logs: configuration summary (sanitize sensitive parts of URLs)
  std::cout << "Config: port=" << config_.port
            << " ical_url=" << sanitizeUrlForLogging(config_.icalUrl)
            << " public_dir=" << config_.publicDir
            << " pending_dir=" << config_.pendingDir << '\n';

  if (!config_.authEnabled) {
    std::cerr << "Warning: AUTH_ENABLED=false — security is disabled (development only).\n";
  } else {
    std::cout << "Auth: enabled; public_base_url=" << (config_.publicBaseUrl.empty() ? "(not set)" : config_.publicBaseUrl) << '\n';
  }

  // Notifier summary: list configured notifier backends
  std::cout << "Notifiers:";
  for (const auto& n : notifiers_) {
    try {
      std::cout << ' ' << n->name();
    } catch (...) {
      std::cout << " unknown";
    }
  }
  std::cout << '\n';

  configureRoutes();

  if (fs::exists(config_.publicDir) && fs::is_directory(config_.publicDir)) {
    server_.set_mount_point("/", config_.publicDir.c_str());
  } else {
    std::cerr << "Warning: PUBLIC_DIR does not exist: " << config_.publicDir << '\n';
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

  std::cout << "Birthday calendar server on 0.0.0.0 port " << config_.port
            << " (http://0.0.0.0:" << config_.port << "/)" << std::endl;
  // When authentication is enabled, always start a separate listener that
  // serves the authenticated URL endpoint. Bind to 0.0.0.0:9001.
  // When authentication is disabled (development), register the endpoint on
  // the main server for convenience.
  if (config_.authEnabled) {
    const char* authBindEnv = std::getenv("AUTH_URL_BIND");
    const std::string authBind = authBindEnv ? std::string(authBindEnv) : std::string("127.0.0.1");
    const int authPort = static_cast<int>(config_.authUrlPort);
    std::thread([this, authBind, authPort]() {
      httplib::Server authSrv;
      // Ensure static assets and API routes served by the auth listener include CORS headers
      authSrv.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        addCorsHeaders(res);
        if (req.method == "OPTIONS") {
          return httplib::Server::HandlerResponse::Unhandled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
      });
      authSrv.Get("/api/auth/authUrl", [this](const httplib::Request& req, httplib::Response& res) {
        if (!authService_.isLocalAuthClient(req)) {
          res.status = 403;
          res.set_content(R"({"error":"Authenticated URL endpoint is only available from local networks"})", "application/json");
          return;
        }
        handleAuthUrl(req, res);
      });
      // Allow CORS preflight for API routes on the auth listener
      authSrv.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        addCorsHeaders(res);
        res.status = 204;
      });

      // Expose the exchange endpoint on the auth listener so pages served
      // from this listener can complete authentication without cross-origin
      // failures.
      authSrv.Post("/api/auth/exchange", [this](const httplib::Request& req, httplib::Response& res) {
        handleAuthExchange(req, res);
      });
      // Serve the frontend auth page (and assets) from the auth-only listener
      if (fs::exists(config_.authPublicDir) && fs::is_directory(config_.authPublicDir)) {
        authSrv.set_mount_point("/", config_.authPublicDir.c_str());
      } else {
        std::cerr << "Warning: AUTH public dir does not exist: " << config_.authPublicDir << '\n';
      }

      authSrv.set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
        // For non-API routes, serve index.html from the public dir so SPA routing works
        if (res.status != 404 || req.path.rfind("/api", 0) == 0) {
          return;
        }

        const fs::path basePath = fs::path(config_.authPublicDir);
        auto html = readTextFile(basePath / "index.html");
        if (!html.has_value()) {
          html = readTextFile(basePath / "auth.html");
        }
        if (!html.has_value()) {
          return;
        }

        res.status = 200;
        res.set_content(html.value(), "text/html");
      });
      authSrv.listen(authBind.c_str(), authPort);
    }).detach();
    std::cout << "authUrl server on " << authBind << " port " << authPort
          << " (http://" << authBind << ":" << authPort << "/) endpoint: /api/auth/authUrl" << std::endl;
  } else {
    // When auth is disabled, expose the endpoint on the main server for
    // development convenience.
    server_.Get("/api/auth/authUrl", [this](const httplib::Request& req, httplib::Response& res) {
      handleAuthUrl(req, res);
    });
    std::cout << "authUrl endpoint registered on main server (auth disabled)" << std::endl;
  }

  try {
    server_.listen("0.0.0.0", config_.port);
  } catch (const std::exception& ex) {
    std::cerr << "Error: server failed to start: " << ex.what() << std::endl;
    return 1;
  }
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

  // Note: `/api/auth/authUrl` is intentionally NOT registered here. It is either
  // registered on the main server (below) or served from a separate auth-only
  // listener if `AUTH_URL_PORT` is set in the environment.

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

  auto feed = icalFeedService_.fetchBirthdays();
  std::set<std::string> monthDays;
  json payload = json::array();

  if (feed) {
    for (const auto& event : *feed) {
      monthDays.insert(event.monthDay);
      payload.push_back({
          {"name", event.name},
          {"date", event.date},
          {"monthDay", event.monthDay},
      });
    }
  } else {
    // Keep API usable even when remote iCal is down/misconfigured.
    res.set_header("X-Calendar-Warning", feed.error());
  }

  vcardFeedService_.cleanupImportedVcards(monthDays);
  vcardFeedService_.appendPendingBirthdays(monthDays, payload);

  res.set_content(payload.dump(), "application/json");
}

void BirthdayServer::handleAuthUrl(const httplib::Request& req,
                                   httplib::Response& res) {
  addCorsHeaders(res);

  if (!authService_.isLocalAuthClient(req)) {
    res.status = 403;
    res.set_content(R"({"error":"Authenticated token endpoint is only available from local networks"})", "application/json");
    return;
  }

  const JwtTokenInfo token = authService_.currentAuthToken();
  const std::string authUrl = buildAuthUrl(token.token, config_, req);
  res.set_header("Cache-Control", "no-store");
  res.set_content(
      json({
          {"token", token.token},
          {"expiresAt", token.expiresAtEpoch},
        {"authUrl", authUrl},
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
  if (qrToken.empty() || !authService_.validateAuthToken(qrToken)) {
    res.status = 401;
    res.set_content(R"({"error":"Invalid token"})", "application/json");
    return;
  }

  const JwtTokenInfo session = authService_.issueSessionToken();
  const bool secureCookie = config_.publicBaseUrl.rfind("https://", 0) == 0;
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
  const auto [statusCode, response] = vcardWorkflow_.submit(req.body);
  res.status = statusCode;
  res.set_content(response.dump(), "application/json");
}
