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

  JwtTokenInfo currentQrToken() const;
  JwtTokenInfo issueSessionToken() const;
  bool validateQrToken(const std::string& token) const;
  bool validateSessionToken(const std::string& token) const;
  std::string buildAuthCookie(const std::string& token, bool secure, uint32_t maxAgeSeconds) const;
  std::optional<std::string> extractTokenFromRequest(const httplib::Request& req) const;
  bool isLocalQrClient(const httplib::Request& req) const;

 private:
  std::string ensureSigningSecret() const;
  std::string createToken(int64_t issuedAtEpoch,
                          int64_t expiresAtEpoch,
                          const std::string& tokenKind,
                          int64_t windowAnchorEpoch = 0) const;
  bool validateTokenWithKind(const std::string& token, const std::string& expectedKind) const;
  std::string sign(const std::string& content) const;
  bool parseAndValidatePayload(const std::string& payloadJson,
                               const std::string& expectedKind) const;

  AppConfig config_;
  std::string signingSecret_;
};
