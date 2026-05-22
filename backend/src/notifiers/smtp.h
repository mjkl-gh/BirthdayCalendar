#pragma once

#include <filesystem>
#include <string>

#include "../models.h"
#include "notifiers/notifier.h"

class SmtpNotifier final : public Notifier {
 public:
  explicit SmtpNotifier(AppConfig config);

  bool sendVcard(const std::string& subject,
                 const std::string& message,
                 const std::filesystem::path& attachmentPath) override;

 private:
  AppConfig config_;
};
