#include "notifiers/smtp.h"

#include <curl/curl.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "services/vcard.h"

SmtpNotifier::SmtpNotifier(AppConfig config) : config_(std::move(config)) {}

void SmtpNotifier::sendVcard(const Vcard& submission) {
  if (config_.smtpUser.empty() || config_.smtpPass.empty() ||
      config_.smtpFrom.empty() || config_.smtpTo.empty()) {
    throw std::runtime_error("SmtpNotifier: SMTP credentials are not configured");
  }

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    throw std::runtime_error("SmtpNotifier: failed to initialise libcurl");
  }

  const std::string subject = "Birthday vCard submission";
  const std::string vcard = buildVcard(submission);

  std::stringstream body;
  body << "A new birthday request was submitted.\n";
  body << "Name: " << submission.firstName << ' ' << submission.lastName << "\n";
  body << "Email: " << submission.email << "\n";
  body << "Birthday: " << submission.birthday << "\n";

  std::string url = "smtp://" + config_.smtpHost + ":" + std::to_string(config_.smtpPort);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERNAME, config_.smtpUser.c_str());
  curl_easy_setopt(curl, CURLOPT_PASSWORD, config_.smtpPass.c_str());
  curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

  std::string fromAddress = "<" + config_.smtpFrom + ">";
  curl_easy_setopt(curl, CURLOPT_MAIL_FROM, fromAddress.c_str());

  struct curl_slist* recipients = nullptr;
  std::string toAddress = "<" + config_.smtpTo + ">";
  recipients = curl_slist_append(recipients, toAddress.c_str());
  curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

  curl_mime* mime = curl_mime_init(curl);

  curl_mimepart* part = curl_mime_addpart(mime);
  std::string headers = "To: " + config_.smtpTo + "\r\n" +
                        "From: " + config_.smtpFrom + "\r\n" +
                        "Subject: " + subject + "\r\n";
  curl_mime_data(part, headers.c_str(), CURL_ZERO_TERMINATED);

  part = curl_mime_addpart(mime);
  curl_mime_type(part, "text/plain; charset=utf-8");
  curl_mime_data(part, body.str().c_str(), CURL_ZERO_TERMINATED);

  part = curl_mime_addpart(mime);
  curl_mime_type(part, "text/vcard");
  curl_mime_data(part, vcard.c_str(), CURL_ZERO_TERMINATED);
  curl_mime_filename(part, "birthday.vcf");

  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

  const CURLcode result = curl_easy_perform(curl);

  curl_mime_free(mime);
  curl_slist_free_all(recipients);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK) {
    throw std::runtime_error(std::string("SmtpNotifier: curl_easy_perform failed: ") +
                             curl_easy_strerror(result));
  }
}

std::string SmtpNotifier::name() const {
  return "smtp";
}
