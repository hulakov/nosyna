#pragma once

#include "common.h"
#include "mqtt/client.h"

#include <esp_log.h>

class Light
{
  public:
    Light() = delete;
    Light(const Light &) = delete;
    Light &operator=(const Light &) = delete;

    Light(mqtt::Client &mqtt_client, const std::string id, int pin) : m_mqtt(mqtt_client), ID(id), Pin(pin)
    {
    }

    void set_state(bool new_state)
    {
        ESP_LOGI(CONTROLS_LOG_TAG, "Light GPIO %d (%s) state: %d -> %d", Pin, ID.c_str(), m_state, new_state);
        m_state = new_state;
        analogWrite(Pin, m_state ? m_brightness : 0);
        m_mqtt.set(ID, mqtt::prop::STATE, m_state ? mqtt::state::ON : mqtt::state::OFF);
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
    }

  public:
    const std::string ID;
    const int Pin;

  private:
    mqtt::Client &m_mqtt;
    bool m_state = false;
    int m_brightness = mqtt::brightness::MAX;
};
