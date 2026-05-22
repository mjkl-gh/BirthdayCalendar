#include "services/vcard.h"

#include <sstream>
#include <string>

#include "utils/text.h"

// Escape a vCard TEXT value per RFC 6350 §3.4:
// strip bare CR/LF, then escape \, ; and ,
static std::string escapeVcardText(std::string value) {
  // Remove bare CR and LF characters that would break the line structure
  std::string result;
  result.reserve(value.size());
  for (char c : value) {
    if (c == '\r' || c == '\n') continue;
    result += c;
  }
  // Escape backslash first, then semicolon and comma
  std::string out;
  out.reserve(result.size() + 8);
  for (char c : result) {
    if (c == '\\')      { out += "\\\\"; }
    else if (c == ';')  { out += "\\;";  }
    else if (c == ',')  { out += "\\,";  }
    else                { out += c;      }
  }
  return out;
}

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

std::string buildVcard(const Vcard& submission) {
  std::string firstName = escapeVcardText(submission.firstName);
  std::string lastName = escapeVcardText(submission.lastName);
  std::string fullName = escapeVcardText(trim(submission.firstName + " " + submission.lastName));
  std::string email = escapeVcardText(submission.email);
  std::string birthday = submission.birthday;  // validated as YYYY-MM-DD; no escape needed
  std::string notes = escapeVcardText(submission.notes);

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
