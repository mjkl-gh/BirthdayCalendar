#pragma once

#include <memory>
#include <vector>

#include "../models.h"
#include "notifiers/notifier.h"

std::vector<std::unique_ptr<Notifier>> createNotifiers(const AppConfig& config);
