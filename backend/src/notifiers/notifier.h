#pragma once

#include <stdexcept>

#include "models.h"

class Notifier {
 public:
  virtual ~Notifier() = default;
  // Sends the vCard notification. Throws std::runtime_error on failure.
  virtual void sendVcard(const Vcard& submission) = 0;
};
