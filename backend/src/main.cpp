#include <curl/curl.h>

#include <utility>

#include "services/birthday_server.h"
#include "services/config.h"
#include "services/notifierfactory.h"

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  try {
    AppConfig config = loadConfig();
    auto notifiers = createNotifiers(config);

    BirthdayServer server(std::move(config), std::move(notifiers));
    const int exitCode = server.run();

    curl_global_cleanup();
    return exitCode;
  } catch (const std::exception& ex) {
    std::cerr << "Fatal: " << ex.what() << std::endl;
    curl_global_cleanup();
    return 1;
  }
}
