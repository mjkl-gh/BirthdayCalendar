#include "services/notifierfactory.h"

#include <filesystem>
#include <memory>
#include <vector>

#include "notifiers/file.h"
#include "notifiers/smtp.h"

namespace fs = std::filesystem;

std::vector<std::unique_ptr<Notifier>> createNotifiers(const AppConfig& config) {
  std::vector<std::unique_ptr<Notifier>> notifiers;

  if (!config.smtpUser.empty() && !config.smtpPass.empty() &&
      !config.smtpFrom.empty() && !config.smtpTo.empty()) {
    notifiers.push_back(std::make_unique<SmtpNotifier>(config));
  }

  notifiers.push_back(std::make_unique<FileNotifier>(config.pendingDir));
  return notifiers;
}
