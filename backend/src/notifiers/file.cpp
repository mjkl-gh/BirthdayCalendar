#include "notifiers/file.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "utils/file.h"

FileNotifier::FileNotifier(std::filesystem::path outboxDir)
    : outboxDir_(std::move(outboxDir)) {
  std::filesystem::create_directories(outboxDir_);
}

void FileNotifier::sendVcard(const std::string& subject,
                             const std::string& message,
                             const std::filesystem::path& attachmentPath) {
  const auto now = std::chrono::system_clock::now();
  const auto epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
          .count();
  std::filesystem::path outPath =
      outboxDir_ / ("mail-" + std::to_string(epoch) + ".txt");

  std::stringstream data;
  data << "Subject: " << subject << "\n";
  data << "Message:\n" << message << "\n\n";
  data << "Attachment: " << attachmentPath.string() << "\n";

  if (!writeTextFile(outPath, data.str())) {
    throw std::runtime_error("FileNotifier: failed to write to outbox: " + outPath.string());
  }
}
