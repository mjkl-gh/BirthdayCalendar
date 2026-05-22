#include "services/ical.h"

#include <curl/curl.h>

#include <sstream>
#include <string>

#include "utils/text.h"

namespace {

size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  const size_t totalSize = size * nmemb;
  std::string* buffer = static_cast<std::string*>(userp);
  buffer->append(static_cast<char*>(contents), totalSize);
  return totalSize;
}

}  // namespace

std::optional<std::string> fetchRemoteText(const std::string& url) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return std::nullopt;
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  const CURLcode result = curl_easy_perform(curl);
  long statusCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK || statusCode >= 400) {
    return std::nullopt;
  }

  return response;
}

std::vector<BirthdayEvent> parseIcsBirthdays(const std::string& icsText) {
  std::vector<BirthdayEvent> events;
  std::istringstream stream(icsText);
  std::string line;

  bool inEvent = false;
  std::string summary;
  std::string dtstart;

  while (std::getline(stream, line)) {
    line = trim(line);

    if (line == "BEGIN:VEVENT") {
      inEvent = true;
      summary.clear();
      dtstart.clear();
      continue;
    }

    if (!inEvent) {
      continue;
    }

    if (line.rfind("SUMMARY:", 0) == 0) {
      summary = trim(line.substr(8));
      continue;
    }

    if (line.rfind("DTSTART", 0) == 0) {
      dtstart = normalizeDate(line);
      continue;
    }

    if (line == "END:VEVENT") {
      inEvent = false;
      if (!summary.empty() && !dtstart.empty()) {
        BirthdayEvent event{summary, dtstart, toMonthDay(dtstart)};
        if (!event.monthDay.empty()) {
          events.push_back(event);
        }
      }
    }
  }

  return events;
}
