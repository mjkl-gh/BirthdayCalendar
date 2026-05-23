#pragma once

#include <expected>
#include <string>
#include <vector>

#include "../models.h"

class IcalFeedService {
 public:
  explicit IcalFeedService(std::string icalUrl);

  std::expected<std::vector<BirthdayEvent>, std::string> fetchBirthdays() const;

 private:
  std::string icalUrl_;
};
