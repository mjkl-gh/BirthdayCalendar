#pragma once

#include <memory>
#include <string>
#include <vector>

#include <httplib.h>

#include "../models.h"
#include "../notifiers/notifier.h"
#include "services/ical_feed.h"
#include "services/vcard_feed.h"
#include "services/vcard_workflow.h"

class BirthdayServer {
 public:
  BirthdayServer(AppConfig config, std::vector<std::unique_ptr<Notifier>> notifiers);
  int run();

 private:
  void configureRoutes();
  void handleGetBirthdays(const httplib::Request& req, httplib::Response& res);
  void handleCreateVcard(const httplib::Request& req, httplib::Response& res);

  AppConfig config_;
  std::vector<std::unique_ptr<Notifier>> notifiers_;
  IcalFeedService icalFeedService_;
  VcardFeedService vcardFeedService_;
  VcardWorkflow vcardWorkflow_;
  httplib::Server server_;
};
