#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <httplib.h>

#include "models.h"

struct JwtTokenInfo {
  std::string token;
  int64_t expiresAtEpoch;
};

class AuthService {
 public:
  explicit AuthService(const AppConfig& config);

  JwtTokenInfo currentHourlyToken() const;
  bool validateToken(const std::string& token) const;
  std::string buildAuthCookie(const std::string& token, bool secure) const;
  std::optional<std::string> extractTokenFromRequest(const httplib::Request& req) const;
  bool isLocalQrClient(const httplib::Request& req) const;

 private:
  std::string ensureSigningSecret() const;
  std::string createTokenForHour(int64_t hourStartEpoch) const;
  std::string sign(const std::string& content) const;
  bool parseAndValidatePayload(const std::string& payloadJson) const;
  std::string resolveClientIp(const httplib::Request& req) const;

  AppConfig config_;
  std::string signingSecret_;
};
