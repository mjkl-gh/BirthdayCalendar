#include "services/auth.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "utils/file.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

int64_t nowEpochSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::vector<std::string> split(const std::string& input, char delimiter) {
  std::vector<std::string> parts;
  std::stringstream ss(input);
  std::string item;
  while (std::getline(ss, item, delimiter)) {
    parts.push_back(item);
  }
  return parts;
}

std::string trimCopy(const std::string& value) {
  const auto begin = value.find_first_not_of(" \t\n\r");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \t\n\r");
  return value.substr(begin, end - begin + 1);
}

std::string base64UrlEncode(const std::string& raw) {
  if (raw.empty()) {
    return "";
  }

  std::string encoded;
  encoded.resize(4 * ((raw.size() + 2) / 3));
  const int outLen = EVP_EncodeBlock(
      reinterpret_cast<unsigned char*>(&encoded[0]),
      reinterpret_cast<const unsigned char*>(raw.data()),
      static_cast<int>(raw.size()));
  encoded.resize(std::max(0, outLen));

  for (char& c : encoded) {
    if (c == '+') c = '-';
    if (c == '/') c = '_';
  }
  while (!encoded.empty() && encoded.back() == '=') {
    encoded.pop_back();
  }
  return encoded;
}

std::optional<std::string> base64UrlDecode(const std::string& encodedUrl) {
  if (encodedUrl.empty()) {
    return std::string();
  }

  std::string encoded = encodedUrl;
  for (char& c : encoded) {
    if (c == '-') c = '+';
    if (c == '_') c = '/';
  }
  while (encoded.size() % 4 != 0) {
    encoded.push_back('=');
  }

  std::string decoded;
  decoded.resize((encoded.size() * 3) / 4 + 1);
  const int outLen = EVP_DecodeBlock(
      reinterpret_cast<unsigned char*>(&decoded[0]),
      reinterpret_cast<const unsigned char*>(encoded.data()),
      static_cast<int>(encoded.size()));
  if (outLen < 0) {
    return std::nullopt;
  }

  size_t padding = 0;
  if (!encoded.empty() && encoded.back() == '=') padding++;
  if (encoded.size() > 1 && encoded[encoded.size() - 2] == '=') padding++;
  decoded.resize(static_cast<size_t>(outLen) - padding);
  return decoded;
}

bool isIpv4Local(const std::string& ip, const std::string& cidr) {
  const auto slashPos = cidr.find('/');
  if (slashPos == std::string::npos) {
    return ip == cidr;
  }

  const std::string baseIp = cidr.substr(0, slashPos);
  const int prefixBits = std::stoi(cidr.substr(slashPos + 1));

  auto parseIpv4 = [](const std::string& value) -> std::optional<uint32_t> {
    const auto parts = split(value, '.');
    if (parts.size() != 4) return std::nullopt;
    uint32_t out = 0;
    for (const std::string& part : parts) {
      if (part.empty() || part.size() > 3) return std::nullopt;
      for (char c : part) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
      }
      int n = std::stoi(part);
      if (n < 0 || n > 255) return std::nullopt;
      out = (out << 8) | static_cast<uint32_t>(n);
    }
    return out;
  };

  const auto ipNum = parseIpv4(ip);
  const auto baseNum = parseIpv4(baseIp);
  if (!ipNum.has_value() || !baseNum.has_value()) {
    return false;
  }

  if (prefixBits <= 0) return true;
  if (prefixBits >= 32) return *ipNum == *baseNum;
  const uint32_t mask = 0xFFFFFFFFu << (32 - prefixBits);
  return ((*ipNum & mask) == (*baseNum & mask));
}

bool isIpv6Loopback(const std::string& ip) {
  return ip == "::1";
}

std::string toHex(const std::vector<unsigned char>& bytes) {
  static const char* kHex = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (unsigned char b : bytes) {
    out.push_back(kHex[(b >> 4) & 0x0F]);
    out.push_back(kHex[b & 0x0F]);
  }
  return out;
}

}  // namespace

AuthService::AuthService(const AppConfig& config)
    : config_(config), signingSecret_(ensureSigningSecret()) {}

JwtTokenInfo AuthService::currentHourlyToken() const {
  const int64_t now = nowEpochSeconds();
  const int64_t hourStart = now - (now % 3600);
  return {
      .token = createTokenForHour(hourStart),
      .expiresAtEpoch = hourStart + static_cast<int64_t>(config_.jwtTokenLifetimeSeconds + config_.jwtRotationGraceSeconds),
  };
}

bool AuthService::validateToken(const std::string& token) const {
  const auto parts = split(token, '.');
  if (parts.size() != 3) {
    return false;
  }

  const std::string signingInput = parts[0] + "." + parts[1];
  const std::string expectedSig = sign(signingInput);
  if (expectedSig.size() != parts[2].size()) {
    return false;
  }
  if (CRYPTO_memcmp(expectedSig.data(), parts[2].data(), expectedSig.size()) != 0) {
    return false;
  }

  auto payload = base64UrlDecode(parts[1]);
  if (!payload.has_value()) {
    return false;
  }
  return parseAndValidatePayload(payload.value());
}

std::string AuthService::buildAuthCookie(const std::string& token, bool secure) const {
  std::stringstream cookie;
  cookie << config_.jwtCookieName << "=" << token
         << "; Path=/; HttpOnly; SameSite=Lax";
  if (secure) {
    cookie << "; Secure";
  }
  return cookie.str();
}

std::optional<std::string> AuthService::extractTokenFromRequest(const httplib::Request& req) const {
  const std::string authHeader = req.get_header_value("Authorization");
  if (!authHeader.empty()) {
    static const std::string prefix = "Bearer ";
    if (authHeader.rfind(prefix, 0) == 0 && authHeader.size() > prefix.size()) {
      return authHeader.substr(prefix.size());
    }
  }

  const std::string cookieHeader = req.get_header_value("Cookie");
  if (cookieHeader.empty()) {
    return std::nullopt;
  }

  const auto chunks = split(cookieHeader, ';');
  for (const std::string& chunk : chunks) {
    const std::string part = trimCopy(chunk);
    const std::string key = config_.jwtCookieName + "=";
    if (part.rfind(key, 0) == 0 && part.size() > key.size()) {
      return part.substr(key.size());
    }
  }
  return std::nullopt;
}

bool AuthService::isLocalQrClient(const httplib::Request& req) const {
  const std::string ip = resolveClientIp(req);
  if (ip.empty()) {
    return false;
  }

  const auto cidrs = split(config_.localQrAllowedCidrs, ',');
  for (const std::string& cidrRaw : cidrs) {
    const std::string cidr = trimCopy(cidrRaw);
    if (cidr.empty()) continue;

    if (cidr.find(':') != std::string::npos) {
      if (cidr == "::1/128" && isIpv6Loopback(ip)) return true;
      if (cidr == ip) return true;
      continue;
    }

    if (isIpv4Local(ip, cidr)) {
      return true;
    }
  }
  return false;
}

std::string AuthService::ensureSigningSecret() const {
  if (!config_.jwtSecret.empty()) {
    return config_.jwtSecret;
  }

  auto existing = readTextFile(config_.jwtSecretFile);
  if (existing.has_value()) {
    const std::string secret = trimCopy(existing.value());
    if (!secret.empty()) {
      return secret;
    }
  }

  std::vector<unsigned char> bytes(32);
  if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
    throw std::runtime_error("Failed to generate JWT secret");
  }

  const std::string generated = toHex(bytes);
  fs::create_directories(fs::path(config_.jwtSecretFile).parent_path());
  if (!writeTextFile(config_.jwtSecretFile, generated + "\n")) {
    throw std::runtime_error("Failed to persist generated JWT secret");
  }
  return generated;
}

std::string AuthService::createTokenForHour(int64_t hourStartEpoch) const {
  const std::string header = R"({"alg":"HS256","typ":"JWT"})";

  json payload = {
      {"iss", config_.jwtIssuer},
      {"iat", hourStartEpoch},
      {"exp", hourStartEpoch + static_cast<int64_t>(config_.jwtTokenLifetimeSeconds + config_.jwtRotationGraceSeconds)},
      {"hr", hourStartEpoch},
  };

  const std::string encodedHeader = base64UrlEncode(header);
  const std::string encodedPayload = base64UrlEncode(payload.dump());
  const std::string signingInput = encodedHeader + "." + encodedPayload;
  return signingInput + "." + sign(signingInput);
}

std::string AuthService::sign(const std::string& content) const {
  unsigned int len = 0;
  unsigned char digest[EVP_MAX_MD_SIZE] = {0};

  HMAC(
      EVP_sha256(),
      signingSecret_.data(),
      static_cast<int>(signingSecret_.size()),
      reinterpret_cast<const unsigned char*>(content.data()),
      content.size(),
      digest,
      &len);

  return base64UrlEncode(std::string(reinterpret_cast<char*>(digest), len));
}

bool AuthService::parseAndValidatePayload(const std::string& payloadJson) const {
  try {
    const json payload = json::parse(payloadJson);
    const std::string issuer = payload.value("iss", "");
    const int64_t iat = payload.value("iat", static_cast<int64_t>(0));
    const int64_t exp = payload.value("exp", static_cast<int64_t>(0));

    if (issuer != config_.jwtIssuer || iat <= 0 || exp <= 0) {
      return false;
    }

    const int64_t now = nowEpochSeconds();
    if (now > exp) {
      return false;
    }

    const int64_t oldestIat = now - static_cast<int64_t>(config_.jwtTokenLifetimeSeconds + config_.jwtRotationGraceSeconds);
    if (iat < oldestIat || iat > now + 60) {
      return false;
    }

    return true;
  } catch (...) {
    return false;
  }
}

std::string AuthService::resolveClientIp(const httplib::Request& req) const {
  if (config_.trustProxy) {
    const std::string forwardedFor = req.get_header_value("X-Forwarded-For");
    if (!forwardedFor.empty()) {
      auto items = split(forwardedFor, ',');
      if (!items.empty()) {
        return trimCopy(items.front());
      }
    }
  }
  return req.remote_addr;
}
