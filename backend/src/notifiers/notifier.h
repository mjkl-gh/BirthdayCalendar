#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

class Notifier {
 public:
  virtual ~Notifier() = default;
  // Sends the vCard notification. Throws std::runtime_error on failure.
  virtual void sendVcard(const std::string& subject,
                         const std::string& message,
                         const std::filesystem::path& attachmentPath) = 0;
};
