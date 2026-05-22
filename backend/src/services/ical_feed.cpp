#include "services/ical_feed.h"

#include <optional>
#include <utility>
#include <vector>

#include "services/ical.h"

IcalFeedService::IcalFeedService(std::string icalUrl)
    : icalUrl_(std::move(icalUrl)) {}

IcalFeedResult IcalFeedService::fetchBirthdays() const {
  if (icalUrl_.empty()) {
    return {
        .ok = false,
        .statusCode = 500,
        .error = "ICAL_URL is not configured",
        .birthdays = {},
    };
  }

  std::optional<std::string> ics = fetchRemoteText(icalUrl_);
  if (!ics.has_value()) {
    return {
        .ok = false,
        .statusCode = 502,
        .error = "Could not fetch iCal feed",
        .birthdays = {},
    };
  }

  return {
      .ok = true,
      .statusCode = 200,
      .error = "",
      .birthdays = parseIcsBirthdays(ics.value()),
  };
}
