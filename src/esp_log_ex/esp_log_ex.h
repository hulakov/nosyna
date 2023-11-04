#pragma once

#include <functional>

#include <esp_log.h>

typedef std::function<void(const std::string &message)> Appender;

void initialize_log();

void add_log_appender(Appender appender);
void add_mqtt_log_appender(const std::string &device_id, const std::string &hostname, uint16_t port,
                           const std::string &user, const std::string &password);
