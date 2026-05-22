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

  // Enforce http:// or https:// to prevent SSRF via file:// / ftp:// etc.
  if (icalUrl_.rfind("https://", 0) != 0 && icalUrl_.rfind("http://", 0) != 0) {
    return {
        .ok = false,
        .statusCode = 500,
        .error = "ICAL_URL must use http:// or https://",
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
