#pragma once

#include <memory>

#include "../models.h"
#include "notifiers/notifier.h"

std::unique_ptr<Notifier> createNotifier(const AppConfig& config);
