#include "services/notifierfactory.h"

#include <filesystem>
#include <memory>

#include "notifiers/file.h"
#include "notifiers/smtp.h"

namespace fs = std::filesystem;

std::unique_ptr<Notifier> createNotifier(const AppConfig& config) {
  if (!config.smtpUser.empty() && !config.smtpPass.empty() &&
      !config.smtpFrom.empty() && !config.smtpTo.empty()) {
    return std::make_unique<SmtpNotifier>(config);
  }

  fs::path outbox = fs::path(config.pendingDir).parent_path() / "outbox";
  return std::make_unique<FileNotifier>(outbox);
}
