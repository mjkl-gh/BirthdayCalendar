#pragma once

#include <string>
#include <vector>

#include "../models.h"

struct IcalFeedResult {
  bool ok;
  int statusCode;
  std::string error;
  std::vector<BirthdayEvent> birthdays;
};

class IcalFeedService {
 public:
  explicit IcalFeedService(std::string icalUrl);

  IcalFeedResult fetchBirthdays() const;

 private:
  std::string icalUrl_;
};
