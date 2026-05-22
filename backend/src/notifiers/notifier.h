#pragma once

#include <filesystem>
#include <string>

class Notifier {
 public:
  virtual ~Notifier() = default;
    virtual bool sendVcard(const std::string& subject,
                                                 const std::string& message,
                                                 const std::filesystem::path& attachmentPath) = 0;
};
