
// #define TAG "myApp"
// #define LOG_LEVEL ESP_LOG_INFO
// #define MY_ESP_LOG_LEVEL ESP_LOG_WARN

#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <vector>

std::vector<Appender> g_appenders;

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