#include "esp_log_ex.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <vector>

constexpr const char *LOG_LOG_TAG = "log";

std::vector<Appender> g_appenders;

WiFiClient g_wifi;
PubSubClient g_pubsub(g_wifi);

std::string format_string(const char *format, va_list args)
{
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);

    if (size < 0)
        return std::string();

    std::string result(static_cast<size_t>(size) + 1, '\0');
    vsnprintf(&result[0], result.size(), format, args);
    result.pop_back();

    result.erase(result.find_last_not_of(" \t\n\r\f\v") + 1);

    return result;
}

static int custom_log_output(const char *format, va_list args)
{
    const auto message = format_string(format, args);
    for (const auto &appender : g_appenders)
        appender(message);

    return 0;
}

void initialize_log()
{
    // https://community.platformio.org/t/redirect-esp32-log-messages-to-sd-card/33734/7
    esp_log_set_vprintf(custom_log_output);
}

void add_log_appender(Appender appender)
{
    g_appenders.push_back(std::move(appender));
}

void add_mqtt_log_appender(const std::string &device_id, const std::string &hostname, uint16_t port,
                           const std::string &user, const std::string &password)
{
    ESP_LOGI(LOG_LOG_TAG, "Connecting to MQTT...");
    g_pubsub.setServer(hostname.c_str(), port);
    g_pubsub.setBufferSize(1024);
    while (!g_pubsub.connect(device_id.c_str(), user.c_str(), password.c_str()))
    {
        delay(500);
        ESP_LOGI(LOG_LOG_TAG, "Waiting MQTT...");
    }

    ESP_LOGI(LOG_LOG_TAG, "Connected to MQTT");

    add_log_appender([](const std::string &message) { g_pubsub.publish("logs/nosyna", message.c_str()); });
}