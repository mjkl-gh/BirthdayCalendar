#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../notifiers/notifier.h"

struct VcardSubmitResult {
  int statusCode;
  std::string body;
};

class VcardWorkflow {
 public:
  explicit VcardWorkflow(std::vector<std::unique_ptr<Notifier>>& notifiers);

  VcardSubmitResult submit(const std::string& requestBody) const;

 private:
  std::vector<std::unique_ptr<Notifier>>& notifiers_;
};
