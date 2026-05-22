#pragma once

#include <filesystem>

#include "../models.h"
#include "notifiers/notifier.h"

class SmtpNotifier final : public Notifier {
 public:
  explicit SmtpNotifier(AppConfig config);

  void sendVcard(const Vcard& submission) override;

 private:
  AppConfig config_;
};
