#include "services/config.h"

#include <cstdlib>
#include <string>

namespace {

std::string getEnvOr(const char* key, const std::string& fallback = "") {
  const char* value = std::getenv(key);
  if (value == nullptr) {
    return fallback;
  }
  return value;
}

uint32_t getEnvIntOr(const char* key, uint32_t fallback) {
  const char* value = std::getenv(key);
  if (value == nullptr) {
    return fallback;
  }
  try {
    return static_cast<uint32_t>(std::stoul(value));
  } catch (...) {
    return fallback;
  }
}

}  // namespace

AppConfig loadConfig() {
  return {
    .publicDir = getEnvOr("PUBLIC_DIR", "../frontend/dist"),
      .port = getEnvIntOr("PORT", 8080),
      .icalUrl = getEnvOr("ICAL_URL", ""),
      .pendingDir = getEnvOr("PENDING_DIR", "./storage/pending"),
      .smtpHost = getEnvOr("SMTP_HOST", "smtp.gmail.com"),
      .smtpPort = getEnvIntOr("SMTP_PORT", 587),
      .smtpUser = getEnvOr("SMTP_USERNAME", ""),
      .smtpPass = getEnvOr("SMTP_PASSWORD", ""),
      .smtpFrom = getEnvOr("MAIL_FROM", ""),
      .smtpTo = getEnvOr("MAIL_TO", ""),
      .authEnabled = getEnvOr("AUTH_ENABLED", "true") == "true",
      .publicBaseUrl = getEnvOr("PUBLIC_BASE_URL", ""),
      .jwtSecret = getEnvOr("JWT_SECRET", ""),
      .jwtSecretFile = getEnvOr("JWT_SECRET_FILE", "./storage/.jwt_secret"),
      .jwtIssuer = getEnvOr("JWT_ISSUER", "birthday-calendar"),
      .jwtCookieName = getEnvOr("JWT_COOKIE_NAME", "birthday_auth"),
      .jwtRotationGraceSeconds = getEnvIntOr("JWT_ROTATION_GRACE_SECONDS", 30),
      .jwtTokenLifetimeSeconds = getEnvIntOr("JWT_TOKEN_LIFETIME_SECONDS", 30),
      .jwtSessionLifetimeSeconds = getEnvIntOr("JWT_SESSION_LIFETIME_SECONDS", 3600),
      .localQrAllowedCidrs = getEnvOr("LOCAL_QR_ALLOWED_CIDRS", "127.0.0.1/32,::1/128,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16"),
  };
}
