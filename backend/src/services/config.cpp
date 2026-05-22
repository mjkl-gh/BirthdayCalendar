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
      .port = getEnvIntOr("PORT", 8080),
      .icalUrl = getEnvOr("ICAL_URL", ""),
      .pendingDir = getEnvOr("PENDING_DIR", "./storage/pending"),
      .smtpHost = getEnvOr("SMTP_HOST", "smtp.gmail.com"),
      .smtpPort = getEnvIntOr("SMTP_PORT", 587),
      .smtpUser = getEnvOr("SMTP_USERNAME", ""),
      .smtpPass = getEnvOr("SMTP_PASSWORD", ""),
      .smtpFrom = getEnvOr("MAIL_FROM", ""),
      .smtpTo = getEnvOr("MAIL_TO", ""),
  };
}
