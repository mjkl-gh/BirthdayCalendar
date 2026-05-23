#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../notifiers/notifier.h"

using json = nlohmann::json;

class VcardWorkflow {
 public:
  explicit VcardWorkflow(std::vector<std::unique_ptr<Notifier>>& notifiers);

  std::pair<int, json> submit(const std::string& requestBody) const;

 private:
  std::vector<std::unique_ptr<Notifier>>& notifiers_;
};
