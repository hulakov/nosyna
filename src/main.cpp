#include "config.h"
#include "controls/button.h"
#include "controls/light.h"
#include "esp_log_ex/esp_log_ex.h"
#include "mqtt/client.h"
#include "secrets.h"

#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include <sstream>

constexpr const char *NOSYNA_LOG_TAG = "nosyna";

constexpr static const char *LED_LIGHT_ID = "led";
constexpr static const char *BUILTIN_LED_ID = "builtin_led";
constexpr static const char *TEMPERATURE_SENSOR_ID = "temperature";
constexpr static const char *HUMIDITY_SENSOR_ID = "humidity";

std::string get_device_identity();

const std::string g_device_id = "nosyna-" + get_device_identity();
const std::string g_device_name = "Nosyna (" + get_device_identity() + ")";
mqtt::Client g_mqtt_client(MQTT_USERNAME, MQTT_PASSWORD, MQTT_HOSTNAME, MQTT_PORT, g_device_id, g_device_name);

Light led(g_mqtt_client, LED_LIGHT_ID, LED_GPIO);
Button button(BUTTON_GPIO);

void setup_log()
{
    initialize_log();
    add_log_appender([](const std::string &message) { Serial.println(message.c_str()); });
    add_log_appender(
        [](const std::string &message) { g_mqtt_client.get_pubsub().publish("logs/nosyna", message.c_str()); });

    esp_log_level_set(NOSYNA_LOG_TAG, ESP_LOG_VERBOSE);
    esp_log_level_set("*", ESP_LOG_INFO);
}

std::string get_device_identity()
{
    constexpr auto WL_MAC_ADDR_LENGTH = 6;
    const uint16_t length = WL_MAC_ADDR_LENGTH + 1;
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);

    std::string result(static_cast<size_t>(length), '\0');
    snprintf(&result[0], length, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    result.pop_back();

    return result;
}

void setup_serial()
{
    Serial.begin(115200);
}

void setup_wifi(const char *ssid, const char *password)
{
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
    ESP_LOGI(NOSYNA_LOG_TAG, "Setup OTA");

    ArduinoOTA.setHostname(device_name.c_str());
    ArduinoOTA.setPort(3232);
    // ArduinoOTA.setPassword("passwd");

    ArduinoOTA.onStart([]() { ESP_LOGI(NOSYNA_LOG_TAG, "OTA onStart(): command=%d", ArduinoOTA.getCommand()); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        ESP_LOGI(NOSYNA_LOG_TAG, "OTA onProgress: %u/%u", progress, total);
    });
    ArduinoOTA.onEnd([]() { ESP_LOGI(NOSYNA_LOG_TAG, "OTA onEnd()"); });
    ArduinoOTA.onError([](ota_error_t error) { ESP_LOGI(NOSYNA_LOG_TAG, "OTA onError(): error=%d"); });

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
    g_mqtt_client.add_sensor(TEMPERATURE_SENSOR_ID, "Temperature", "temperature", "°C");
    g_mqtt_client.add_sensor(HUMIDITY_SENSOR_ID, "Humidity", "humidity", "%");
    g_mqtt_client.add_switch(BUILTIN_LED_ID, "Built-in LED", "outlet", [](bool on) {
        if (on)
        {
            ESP_LOGI(NOSYNA_LOG_TAG, "Built-in LED ON");
            digitalWrite(LED_BUILTIN, LOW);
            g_mqtt_client.set(BUILTIN_LED_ID, mqtt::prop::STATE, mqtt::state::ON);
        }
        else
        {
            ESP_LOGI(NOSYNA_LOG_TAG, "Built-in LED OFF");
            digitalWrite(LED_BUILTIN, HIGH);
            g_mqtt_client.set(BUILTIN_LED_ID, mqtt::prop::STATE, mqtt::state::OFF);
        }
    });

    g_mqtt_client.add_light(LED_LIGHT_ID, "External LED", "outlet",
                            std::bind(&Light::set_state, &led, std::placeholders::_1),
                            std::bind(&Light::set_brightness, &led, std::placeholders::_1));

    button.set_on_click([]() { led.toggle(); });
}

void log_device_summary()
{
    std::stringstream ss;
    ss << "Device " << g_device_name << " is configured:" << std::endl;
    ss << " - Board: " << ARDUINO_BOARD << std::endl;
    ss << " - IP: " << WiFi.localIP() << std::endl;
    ss << " - Flash memory: " << ESP.getFlashChipSize();
    ESP_LOGI(NOSYNA_LOG_TAG, "%s", ss.str().c_str());
}

void setup()
{
    setup_log();
    setup_serial();
    setup_wifi(WIFI_SSID, WIFI_PASSWORD);
    g_mqtt_client.setup();
    setup_ota(g_device_name.c_str());
    setup_pins();
    setup_entities();
    log_device_summary();
}

void loop()
{
    ArduinoOTA.handle();
    g_mqtt_client.loop();
    button.loop();
}
