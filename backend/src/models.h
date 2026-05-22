#pragma once

#include <cstdint>
#include <string>
#include <filesystem>

struct BirthdayEvent {
  std::string name;
  std::string date;
  std::string monthDay;
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

};
