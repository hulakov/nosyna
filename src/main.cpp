#include "config.h"
#include "secrets.h"

#include "controls/button.h"
#include "controls/common.h"
#include "controls/light.h"
#include "controls/temperature_and_humidity.h"

#include "esp_log_ex/esp_log_ex.h"
#include "mqtt/client.h"

#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WiFi.h>

constexpr const char *NOSYNA_LOG_TAG = "nosyna";

constexpr static const char *LED_LIGHT_ID = "led";
constexpr static const char *BUILTIN_LED_ID = "builtin_led";
constexpr static const char *TEMPERATURE_SENSOR_ID = "temperature";
constexpr static const char *HUMIDITY_SENSOR_ID = "humidity";

std::string get_device_mac();

const std::string g_mac = get_device_mac();
const std::string g_device_id = "nosyna-" + g_mac;
const std::string g_device_name = "Nosyna (" + g_mac + ")";
mqtt::Client g_mqtt_client(MQTT_USERNAME, MQTT_PASSWORD, MQTT_HOSTNAME, MQTT_PORT, g_device_id, g_device_name);
Preferences g_preferences;

Light g_led(g_mqtt_client, g_preferences, LED_LIGHT_ID, "External LED", LED_GPIO);
Button g_button(BUTTON_GPIO);
TemperatureAndHumidity g_temperature_and_humidity(g_mqtt_client, TEMPERATURE_SENSOR_ID, HUMIDITY_SENSOR_ID,
                                                  "Sensor T&H", TEMPERATURE_AND_HUMIDITY_GPIO);

void setup_log()
{
    initialize_log();
    add_log_appender([](const std::string &message) { Serial.println(message.c_str()); });
    add_log_appender(
        [](const std::string &message) { g_mqtt_client.get_pubsub().publish("logs/nosyna", message.c_str()); });

    esp_log_level_set(NOSYNA_LOG_TAG, ESP_LOG_INFO);
    esp_log_level_set(CONTROLS_LOG_TAG, ESP_LOG_INFO);
    esp_log_level_set(MQTT_LOG_TAG, ESP_LOG_INFO);
    esp_log_level_set("*", ESP_LOG_INFO);
}

std::string get_device_mac()
{
    constexpr auto WL_MAC_ADDR_LENGTH = 6;
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);

    constexpr uint16_t length = WL_MAC_ADDR_LENGTH * 2 + 1;
    char buffer[length];
    snprintf(buffer, length, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return buffer;
}

void setup_serial()
{
    Serial.begin(115200);
}

void setup_wifi(const char *ssid, const char *password)
{
    WiFi.setHostname(g_device_id.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    ESP_LOGI(NOSYNA_LOG_TAG, "Connecting to WIFI '%s'...", ssid);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        ESP_LOGI(NOSYNA_LOG_TAG, "Waiting WIFI...");
    }

    ESP_LOGI(NOSYNA_LOG_TAG, "Connected to WiFI: SSID='%s', IP=%s", ssid, WiFi.localIP().toString().c_str());
}

void setup_ota(const std::string &device_name)
{
    ESP_LOGD(NOSYNA_LOG_TAG, "Setup OTA");

    ArduinoOTA.setHostname(device_name.c_str());
    ArduinoOTA.setPort(3232);
    // ArduinoOTA.setPassword("passwd");

    ArduinoOTA.onStart([]() { ESP_LOGI(NOSYNA_LOG_TAG, "Start OTA update: command=%d", ArduinoOTA.getCommand()); });
    auto last_percent = std::make_shared<unsigned int>(-1);
    ArduinoOTA.onProgress([last_percent](unsigned int progress, unsigned int total) {
        unsigned int percent = 100 * progress / total;
        if (percent != *last_percent)
        {
            *last_percent = percent;
            ESP_LOGI(NOSYNA_LOG_TAG, "OTA %u%%", percent);
        }
    });
    ArduinoOTA.onEnd([]() { ESP_LOGI(NOSYNA_LOG_TAG, "OTA finisted"); });
    ArduinoOTA.onError([](ota_error_t error) { ESP_LOGI(NOSYNA_LOG_TAG, "OTA error: error=%d"); });

    ArduinoOTA.begin();

    ESP_LOGI(NOSYNA_LOG_TAG, "OTA configured");
}

void setup_pins()
{
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_GPIO, OUTPUT);
    pinMode(BUTTON_GPIO, INPUT_PULLDOWN);
}

void setup_entities()
{
    g_temperature_and_humidity.setup();
    g_mqtt_client.add_switch(BUILTIN_LED_ID, "Built-in LED", "outlet", [](bool on) {
        if (on)
        {
            ESP_LOGI(NOSYNA_LOG_TAG, "Built-in LED ON");
            digitalWrite(LED_BUILTIN, LOW);
            g_mqtt_client.set(BUILTIN_LED_ID, mqtt::prop::STATE, true);
        }
        else
        {
            ESP_LOGI(NOSYNA_LOG_TAG, "Built-in LED OFF");
            digitalWrite(LED_BUILTIN, HIGH);
            g_mqtt_client.set(BUILTIN_LED_ID, mqtt::prop::STATE, false);
        }
    });

    g_led.setup();

    g_button.set_on_click([]() { g_led.toggle(); });
}

void log_device_summary()
{
    std::string message;
    message += "Device " + g_device_name + " is configured:\n";
    message += " - Board: " + std::string(ARDUINO_BOARD) + "\n";
    message += " - IP: " + std::string(WiFi.localIP().toString().c_str()) + "\n";
    message += " - MAC: " + std::string(WiFi.macAddress().c_str()) + "\n";
    message += " - Flash memory: " + std::to_string(ESP.getFlashChipSize() / 1024) + " KB";
    ESP_LOGI(NOSYNA_LOG_TAG, "%s", message.c_str());
}

void setup()
{
    setup_log();
    setup_serial();
    setup_wifi(WIFI_SSID, WIFI_PASSWORD);
    g_mqtt_client.setup();
    g_preferences.begin("nosyna");
    setup_ota(g_device_name.c_str());
    setup_pins();
    setup_entities();
    log_device_summary();
}

void loop()
{
    ArduinoOTA.handle();
    g_mqtt_client.loop();
    g_button.loop();
    g_temperature_and_humidity.loop();
}
