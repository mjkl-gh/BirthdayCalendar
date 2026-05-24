cmake_minimum_required(VERSION 3.20)
project(MagicMirrorCompanionBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.20.0
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(magic_mirror_companion_backend
  src/main.cpp
  src/services/birthday_server.cpp
  src/services/config.cpp
  src/utils/file.cpp
  src/notifiers/file.cpp
  src/services/ical.cpp
  src/services/ical_feed.cpp
  src/services/auth.cpp
  src/services/notifierfactory.cpp
  src/notifiers/smtp.cpp
  src/utils/text.cpp
  src/services/vcard_feed.cpp
  src/services/vcard.cpp
  src/services/vcard_workflow.cpp
)

target_include_directories(magic_mirror_companion_backend PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${httplib_SOURCE_DIR}
)

target_link_libraries(magic_mirror_companion_backend PRIVATE
  nlohmann_json::nlohmann_json
  CURL::libcurl
  OpenSSL::SSL
  OpenSSL::Crypto
)
