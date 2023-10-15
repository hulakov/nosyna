#pragma once

#include "constants.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include <memory>
#include <string>
#include <unordered_map>

constexpr const char *MQTT_LOG_TAG = "mqtt";

namespace mqtt
{

class Client final
{
  public:
    Client(const std::string &user, const std::string &password, const std::string &hostname, uint16_t port,
           const std::string &device_id, const std::string &device_name);

    PubSubClient &get_pubsub();
    void setup();
    void loop();

    void add_sensor(const std::string &id, const std::string &name, const std::string &device_class,
                    const std::string &unit_of_measurement = "");
    void add_switch(const std::string &id, const std::string &name, const std::string &device_class,
                    std::function<void(bool on)> handler);
    void add_light(const std::string &id, const std::string &name, const std::string &device_class,
                   std::function<void(bool on)> state_handler, std::function<void(int value)> brightness_handler);

    void set(const std::string &id, const std::string &property, const std::string &value);
    void set(const std::string &id, const std::string &property, int value);

    void send_pending_states();

  private:
    bool publish(const std::string &topic, const std::string &payload, const std::string &prettyPayload = "");
    bool publish(const std::string &topic, const JsonDocument &payload);
    bool subscribe(const std::string &topic, std::function<void(const std::string &)> handler);

    void connect();
    void callback(char *topic, byte *payload, unsigned int length);

    void fill_device_info(JsonDocument &parentJson) const;

    void set(const std::string &key, const std::string &value);

  private:
    WiFiClient m_wifi;
    PubSubClient m_pubsub;

    std::unordered_map<std::string, std::string> m_values;
    std::unordered_map<std::string, std::string> m_dirty_values;

    std::unordered_map<std::string, std::function<void(const std::string &)>> m_subscriptions;

    std::string m_user;
    std::string m_password;
    std::string m_hostname;
    uint16_t m_port = 0;
    std::string m_device_id;
    std::string m_device_name;
};

} // namespace mqtt