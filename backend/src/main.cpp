#include <curl/curl.h>

#include <utility>

#include "services/birthday_server.h"
#include "services/config.h"
#include "services/notifierfactory.h"

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  AppConfig config = loadConfig();
  auto sender = createNotifier(config);

  BirthdayServer server(std::move(config), std::move(sender));
  const int exitCode = server.run();

  curl_global_cleanup();
  return exitCode;
}
