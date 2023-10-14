#pragma once

#include <functional>

#include <esp_log.h>

typedef std::function<void(const std::string &message)> Appender;

void initialize_log();

void add_log_appender(Appender appender);
