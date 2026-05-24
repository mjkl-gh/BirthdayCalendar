#pragma once

#include <filesystem>

#include "models.h"
#include "notifiers/notifier.h"

class FileNotifier final : public Notifier {
 public:
  explicit FileNotifier(std::filesystem::path pendingDir);

  void sendVcard(const Vcard& submission) override;
  std::string name() const override;

 private:
  std::filesystem::path pendingDir_;
};
