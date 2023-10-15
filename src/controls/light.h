#pragma once

#include "common.h"
#include "mqtt/client.h"

#include <Preferences.h>

#include <esp_log.h>

constexpr const char *LIGHT_STATE_PREFERENCE_KEY = "st";
constexpr const char *LIGHT_BRIGHTNESS_PREFERENCE_KEY = "bri";

class Light
{
  public:
    Light() = delete;
    Light(const Light &) = delete;
    Light &operator=(const Light &) = delete;

    Light(mqtt::Client &mqtt_client, Preferences &preferences, const std::string id, const std::string name, int pin)
        : m_mqtt(mqtt_client), m_preferences(preferences), ID(id), Name(name), Pin(pin)
    {
    }

    std::string get_preferences_key(const std::string &attribute)
    {
        return "light." + std::to_string(Pin) + "." + attribute;
    }

    void setup()
    {
        ESP_LOGD(CONTROLS_LOG_TAG, "Configuring LIGHT GPIO %d (%s)", Pin, ID.c_str());

        m_state = m_preferences.getBool(get_preferences_key(LIGHT_STATE_PREFERENCE_KEY).c_str());
        m_brightness =
            m_preferences.getInt(get_preferences_key(LIGHT_BRIGHTNESS_PREFERENCE_KEY).c_str(), mqtt::brightness::MAX);

        m_mqtt.add_light(ID, Name, "light", std::bind(&Light::set_state, this, std::placeholders::_1),
                         std::bind(&Light::set_brightness, this, std::placeholders::_1));
        m_mqtt.set(ID, mqtt::prop::STATE, m_state);
        m_mqtt.set(ID, mqtt::prop::BRIGHTNESS, m_brightness);
        analogWrite(Pin, m_state ? m_brightness : 0);

        ESP_LOGI(CONTROLS_LOG_TAG, "Light GPIO %d (%s) configured: state %d, brightness %d", Pin, ID.c_str(), m_state,
                 m_brightness);
    }

    void set_state(bool new_state)
    {
        ESP_LOGI(CONTROLS_LOG_TAG, "Light GPIO %d (%s) state: %d -> %d", Pin, ID.c_str(), m_state, new_state);
        m_state = new_state;
        analogWrite(Pin, m_state ? m_brightness : 0);
        m_mqtt.set(ID, mqtt::prop::STATE, m_state);
        m_preferences.putBool(get_preferences_key(LIGHT_STATE_PREFERENCE_KEY).c_str(), m_state);
    }

    void toggle()
    {
        set_state(!m_state);
    }

    void set_brightness(int new_brightness)
    {
        ESP_LOGI(CONTROLS_LOG_TAG, "Light GPIO %d (%s) brightness: %d -> %d", Pin, ID.c_str(), m_brightness,
                 new_brightness);
        m_brightness = new_brightness;
        analogWrite(Pin, m_state ? m_brightness : 0);
        m_mqtt.set(ID, mqtt::prop::BRIGHTNESS, m_brightness);
        m_preferences.putInt(get_preferences_key(LIGHT_BRIGHTNESS_PREFERENCE_KEY).c_str(), m_brightness);
    }

  public:
    const std::string ID;
    const std::string Name;
    const int Pin;

  private:
    mqtt::Client &m_mqtt;
    Preferences &m_preferences;

    bool m_state = false;
    int m_brightness = mqtt::brightness::MAX;
};
