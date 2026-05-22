#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <httplib.h>

#include "../models.h"
#include "../notifiers/notifier.h"

class BirthdayServer {
 public:
  BirthdayServer(AppConfig config, std::vector<std::unique_ptr<Notifier>> notifiers);
  int run();

 private:
  void configureRoutes();
  void handleGetBirthdays(const httplib::Request& req, httplib::Response& res);
  void handleCreateVcard(const httplib::Request& req, httplib::Response& res);
  void cleanupImportedVcards(const std::set<std::string>& currentMonthDays);

  AppConfig config_;
  std::vector<std::unique_ptr<Notifier>> notifiers_;
  httplib::Server server_;
};
