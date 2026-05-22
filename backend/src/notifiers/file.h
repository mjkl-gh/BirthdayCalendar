#pragma once

#include <filesystem>
#include <string>

#include "notifiers/notifier.h"

class FileNotifier final : public Notifier {
 public:
  explicit FileNotifier(std::filesystem::path outboxDir);

  void sendVcard(const std::string& subject,
                 const std::string& message,
                 const std::filesystem::path& attachmentPath) override;

 private:
  std::filesystem::path outboxDir_;
};
