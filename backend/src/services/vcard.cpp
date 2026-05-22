#include "services/vcard.h"

#include <sstream>
#include <string>

#include "utils/text.h"

using json = nlohmann::json;

std::optional<std::string> extractVcardBirthday(const std::string& vcardContent) {
  std::istringstream stream(vcardContent);
  std::string line;
  while (std::getline(stream, line)) {
    line = trim(line);
    if (line.rfind("BDAY", 0) == 0) {
      std::string normalized = normalizeDate(line);
      if (!normalized.empty()) {
        return normalized;
      }
    }
  }
  return std::nullopt;
}

std::string buildVcard(const json& payload) {
  std::string firstName = payload.value("firstName", "");
  std::string lastName = payload.value("lastName", "");
  std::string fullName = trim(firstName + " " + lastName);
  std::string email = payload.value("email", "");
  std::string birthday = payload.value("birthday", "");
  std::string notes = payload.value("notes", "");

  std::stringstream vcf;
  vcf << "BEGIN:VCARD\r\n";
  vcf << "VERSION:3.0\r\n";
  vcf << "N:" << lastName << ';' << firstName << ";;;\r\n";
  vcf << "FN:" << fullName << "\r\n";
  if (!email.empty()) {
    vcf << "EMAIL;TYPE=INTERNET:" << email << "\r\n";
  }
  vcf << "BDAY:" << birthday << "\r\n";
  if (!notes.empty()) {
    vcf << "NOTE:" << notes << "\r\n";
  }
  vcf << "END:VCARD\r\n";
  return vcf.str();
}
