#pragma once

#include <filesystem>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

class VcardFeedService {
 public:
  explicit VcardFeedService(std::filesystem::path pendingDir);

  void cleanupImportedVcards(const std::set<std::string>& currentMonthDays) const;
  void appendPendingBirthdays(std::set<std::string>& monthDays,
                              nlohmann::json& payload) const;

 private:
  std::filesystem::path pendingDir_;
};
