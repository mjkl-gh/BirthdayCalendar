#pragma once

#include <string>

struct BirthdayEvent {
  std::string name;
  std::string date;
  std::string monthDay;
};

struct AppConfig {
  uint32_t port;

  std::string icalUrl;
  
  std::string pendingDir;

  std::string smtpHost;
  uint32_t smtpPort;
  std::string smtpUser;
  std::string smtpPass;
  std::string smtpFrom;
  std::string smtpTo;
  
};
