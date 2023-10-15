#include "client.h"

#include <esp_log.h>

constexpr const char *HOME_ASSISTANT_STATUS = "homeassistant/status";

namespace mqtt
{

Client::Client(const std::string &user, const std::string &password, const std::string &hostname, uint16_t port,
               const std::string &device_id, const std::string &device_name)
    : m_pubsub(m_wifi), m_user(user), m_password(password), m_hostname(hostname), m_port(port), m_device_id(device_id),
      m_device_name(device_name)
{
}

void Client::connect()
{
    ESP_LOGI(MQTT_LOG_TAG, "Connecting to MQTT...");
    while (!m_pubsub.connect(m_device_id.c_str(), m_user.c_str(), m_password.c_str()))
    {
        delay(500);
        ESP_LOGI(MQTT_LOG_TAG, "Waiting MQTT...");
    }

    subscribe(HOME_ASSISTANT_STATUS, [this](const std::string &status) {
        ESP_LOGI(MQTT_LOG_TAG, "Home Assistant went %s", status.c_str());
        if (status == availability::ONLINE)
        {
            m_dirty_values = m_values;
        }
    });

    ESP_LOGI(MQTT_LOG_TAG, "Connected to MQTT");
}

void Client::callback(char *topic, byte *payload, unsigned int length)
{
    const std::string str(reinterpret_cast<const char *>(payload), length);

    auto p = m_subscriptions.find(topic);
    if (p == m_subscriptions.end())
    {
        ESP_LOGW(MQTT_LOG_TAG, "Received unexpected subscrition from topic '%s': %s", topic, str.c_str());
        return;
    }

    ESP_LOGD(MQTT_LOG_TAG, "Received subscrition from topic '%s': %s", topic, str.c_str());
    const auto &handler = p->second;
    handler(str);
}

bool Client::publish(const std::string &topic, const std::string &payload, const std::string &prettyPayload)
{
    if (m_pubsub.publish(topic.c_str(), payload.c_str()))
    {
        ESP_LOGD(MQTT_LOG_TAG, "Publish to topic '%s':\n%s\n", topic.c_str(),
                 prettyPayload.empty() ? payload.c_str() : prettyPayload.c_str());
        return true;
    }
    else
    {
        ESP_LOGE(MQTT_LOG_TAG, "Publish to topic '%s' failed:\n%s\n", topic.c_str(),
                 prettyPayload.empty() ? payload.c_str() : prettyPayload.c_str());
        return true;
    }
}

bool Client::publish(const std::string &topic, const JsonDocument &payload)
{
    std::string str;
    serializeJson(payload, str);

    std::string prettyStr;
    serializeJsonPretty(payload, prettyStr);

    return publish(topic, str, prettyStr);
}

bool Client::subscribe(const std::string &topic, std::function<void(const std::string &)> handler)
{
    if (m_subscriptions.find(topic) != m_subscriptions.end())
    {
        ESP_LOGW(MQTT_LOG_TAG, "Subscription '%s' already exists", topic.c_str());
        return false;
    }

    m_subscriptions[topic] = handler;
    m_pubsub.subscribe(topic.c_str());

    return true;
}

void Client::loop()
{
    if (!m_pubsub.connected())
    {
        ESP_LOGI(MQTT_LOG_TAG, "Connection lost, reconnecting...");
        connect();
    }

    m_pubsub.loop();
    send_pending_states();
}

void Client::setup()
{
    ESP_LOGI(MQTT_LOG_TAG, "Setup MQTT");
    m_pubsub.setBufferSize(1024);
    m_pubsub.setServer(m_hostname.c_str(), m_port);
    m_pubsub.setCallback(
        std::bind(&Client::callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    connect();
}

PubSubClient &Client::get_pubsub()
{
    return m_pubsub;
}

void Client::fill_device_info(JsonDocument &parentJson) const
{
    JsonObject device = parentJson.createNestedObject("device");
    device["name"] = m_device_name;
    device["model"] = ARDUINO_BOARD;
    device["sw_version"] = "0.0.1";
    device["manufacturer"] = "Vadym";

    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(m_device_id);
}

inline std::string make_sensor_discovery_topic(const std::string &component, const std::string &device_id,
                                               const std::string &control_id)
{
    return "homeassistant/" + component + "/" + device_id + "/" + control_id + "/config";
}

inline std::string make_set_topic(const std::string &device_id, const std::string &control_id)
{
    return "nosyna/" + device_id + "/" + control_id + "/set";
}

inline std::string make_set_topic(const std::string &device_id, const std::string &control_id,
                                  const std::string &subtopic)
{
    return "nosyna/" + device_id + "/" + control_id + "/" + subtopic + "/set";
}

inline std::string make_state_topic(const std::string &device_id)
{
    return "nosyna/" + device_id + "/state";
}

void Client::add_sensor(const std::string &id, const std::string &name, const std::string &device_class,
                        const std::string &unit_of_measurement)
{
    StaticJsonDocument<1000> payload;
    payload["name"] = name;
    payload["device_class"] = device_class;
    payload["state_topic"] = "nosyna/" + m_device_id + "/state";
    payload["unique_id"] = m_device_id + "-" + id;
    payload["value_template"] = "{{ value_json." + id + " | is_defined }}";
    if (!unit_of_measurement.empty())
        payload["unit_of_measurement"] = unit_of_measurement;

    fill_device_info(payload);

    const auto discoveryTopic = make_sensor_discovery_topic("sensor", m_device_id, id);
    publish(discoveryTopic, payload);
}

void Client::add_switch(const std::string &id, const std::string &name, const std::string &device_class,
                        std::function<void(bool on)> handler)
{
    const std::string command_topic = make_set_topic(m_device_id, id);
    StaticJsonDocument<1000> payload;
    payload["name"] = name;
    payload["device_class"] = device_class;
    payload["state_topic"] = make_state_topic(m_device_id);
    payload["command_topic"] = command_topic;
    payload["unique_id"] = m_device_id + "-" + id;
    payload["value_template"] = "{{ value_json." + id + "_state | is_defined }}";
    payload["payload_on"] = state::ON;
    payload["payload_off"] = state::OFF;
    payload["state_on"] = state::ON;
    payload["state_off"] = state::OFF;

    fill_device_info(payload);

    const auto discoveryTopic = make_sensor_discovery_topic("switch", m_device_id, id);

    publish(discoveryTopic, payload);
    auto string_handler = [handler](const std::string &value) { handler(value == state::ON); };
    subscribe(command_topic, string_handler);

    handler(false);
}

void Client::add_light(const std::string &id, const std::string &name, const std::string &device_class,
                       std::function<void(bool on)> state_handler, std::function<void(int value)> brightness_handler)
{
    const std::string command_topic = make_set_topic(m_device_id, id);
    const std::string brightness_topic = make_set_topic(m_device_id, id, "brightness");
    StaticJsonDocument<1000> payload;
    payload["unique_id"] = m_device_id + "-" + id;
    payload["name"] = name;
    payload["device_class"] = device_class;

    payload["state_topic"] = make_state_topic(m_device_id);
    payload["command_topic"] = command_topic;
    payload["state_value_template"] = "{{ value_json." + id + "_state | is_defined }}";

    payload["brightness_state_topic"] = make_state_topic(m_device_id);
    payload["brightness_command_topic"] = brightness_topic;
    payload["brightness_value_template"] = "{{ value_json." + id + "_brightness | is_defined }}";

    payload["payload_on"] = state::ON;
    payload["payload_off"] = state::OFF;
    payload["brightness_scale"] = std::to_string(brightness::MAX);

    fill_device_info(payload);

    const auto discoveryTopic = make_sensor_discovery_topic("light", m_device_id, id);

    publish(discoveryTopic, payload);
    subscribe(command_topic, [state_handler](const std::string &value) { state_handler(value == state::ON); });
    subscribe(brightness_topic,
              [brightness_handler](const std::string &value) { brightness_handler(std::stoi(value)); });
}

void Client::set(const std::string &id, const std::string &property, const std::string &value)
{
    const auto key = id + "_" + property;

    const auto p = m_values.find(key);
    if (p != m_values.end() && p->second == value)
        return;

    m_values[key] = value;
    m_dirty_values[key] = value;
}

void Client::set(const std::string &id, const std::string &property, int value)
{
    set(id, property, std::to_string(value));
}

void Client::set(const std::string &id, const std::string &property, bool value)
{
    set(id, property, std::string(value ? state::ON : state::OFF));
}

void Client::send_pending_states()
{
    if (m_dirty_values.empty())
        return;

    StaticJsonDocument<1000> payload;
    for (const auto &p : m_dirty_values)
        payload[p.first] = p.second;
    m_dirty_values.clear();

    const std::string stateTopic = make_state_topic(m_device_id);
    publish(stateTopic, payload);
}

} // namespace mqtt