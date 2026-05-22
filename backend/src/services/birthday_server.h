#pragma once

#include <memory>
#include <set>
#include <string>

#include <httplib.h>

#include "../models.h"
#include "../notifiers/notifier.h"

class BirthdayServer {
 public:
  BirthdayServer(AppConfig config, std::unique_ptr<Notifier> sender);
  int run();

 private:
  void configureRoutes();
  void handleGetBirthdays(const httplib::Request& req, httplib::Response& res);
  void handleCreateVcard(const httplib::Request& req, httplib::Response& res);
  void cleanupImportedVcards(const std::set<std::string>& currentMonthDays);

  AppConfig config_;
  std::unique_ptr<Notifier> sender_;
  httplib::Server server_;
};
