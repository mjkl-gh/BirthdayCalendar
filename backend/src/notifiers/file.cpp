#include "notifiers/file.h"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

#include "services/vcard.h"
#include "utils/file.h"
#include "utils/text.h"

FileNotifier::FileNotifier(std::filesystem::path pendingDir)
    : pendingDir_(std::move(pendingDir)) {
  try {
    std::filesystem::create_directories(pendingDir_);
  } catch (const std::exception& ex) {
    throw std::runtime_error(std::string("FileNotifier: failed to create pending directory '") + pendingDir_.string() + "': " + ex.what());
  }
}

void FileNotifier::sendVcard(const Vcard& submission) {
  const auto now = std::chrono::system_clock::now();
  const auto epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
          .count();
  std::filesystem::path vcfPath = pendingDir_ /
      (sanitizeFileName(submission.firstName + "-" + submission.lastName) +
       "-" + std::to_string(epoch) + ".vcf");

  const std::string vcard = buildVcard(submission);
  if (!writeTextFile(vcfPath, vcard)) {
    throw std::runtime_error("FileNotifier: failed to persist vCard: " + vcfPath.string());
  }
}

std::string FileNotifier::name() const {
  return "file";
}
