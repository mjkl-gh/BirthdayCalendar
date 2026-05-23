#pragma once

#include <cstdint>
#include <string>
#include <filesystem>

struct BirthdayEvent {
  std::string name;
  std::string date;
  std::string monthDay;
};

struct Vcard {
  std::string firstName;
  std::string lastName;
  std::string email;
  std::string birthday;
  std::string notes;
};

struct AppConfig {
  std::filesystem::path publicDir;
  uint32_t port;

  std::string icalUrl;

  std::filesystem::path pendingDir;

  std::string smtpHost;
  uint32_t smtpPort;
  std::string smtpUser;
  std::string smtpPass;
  std::string smtpFrom;
  std::string smtpTo;

  std::string jwtSecret;
  std::filesystem::path jwtSecretFile;
  std::string jwtIssuer;
  std::string jwtCookieName;
  uint32_t jwtRotationGraceSeconds;
  uint32_t jwtTokenLifetimeSeconds;
  uint32_t jwtSessionLifetimeSeconds;
  bool trustProxy;
  std::string localQrAllowedCidrs;

};
