#include "services/ical_feed.h"

#include <optional>
#include <utility>
#include <vector>

#include "services/ical.h"

IcalFeedService::IcalFeedService(std::string icalUrl)
    : icalUrl_(std::move(icalUrl)) {}

std::expected<std::vector<BirthdayEvent>, std::string> IcalFeedService::fetchBirthdays() const {
  if (icalUrl_.empty()) {
    return std::unexpected("ICAL_URL is not configured");
  }

  // Enforce http:// or https:// to prevent SSRF via file:// / ftp:// etc.
  if (icalUrl_.rfind("https://", 0) != 0 && icalUrl_.rfind("http://", 0) != 0) {
    return std::unexpected("ICAL_URL must use http:// or https://");
  }

  std::optional<std::string> ics = fetchRemoteText(icalUrl_);
  if (!ics.has_value()) {
    return std::unexpected("Could not fetch iCal feed");
  }

  return parseIcsBirthdays(ics.value());
}
