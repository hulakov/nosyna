#pragma once

#include "common.h"
#include "mqtt/client.h"

#include <DHT.h>

#include <esp_log.h>

#include <cmath>

class TemperatureAndHumidity
{
  public:
    TemperatureAndHumidity() = delete;
    TemperatureAndHumidity(const TemperatureAndHumidity &) = delete;
    TemperatureAndHumidity &operator=(const TemperatureAndHumidity &) = delete;

    TemperatureAndHumidity(mqtt::Client &mqtt_client, std::string temperature_id, std::string humidity_id,
                           const std::string name, int pin)
        : m_mqtt(mqtt_client), m_dht(pin, DHT22), TemperatureID(temperature_id), HumidityID(humidity_id), Name(name),
          Pin(pin)
    {
    }

    void setup()
    {
        m_mqtt.add_sensor(TemperatureID, Name + " Temperature", "temperature", "°C");
        m_mqtt.add_sensor(HumidityID, Name + " Humidity", "humidity", "%");
        m_dht.begin();
        ESP_LOGI(CONTROLS_LOG_TAG, "Configured temperature and humidity sensor GPIO %d (%s, %s)", Pin,
                 TemperatureID.c_str(), HumidityID.c_str());
    }

    void loop()
    {
        int now = millis();
        if (now - m_last_update_ms < 1000)
        {
            return;
        }
        m_last_update_ms = now;

        float temperature = m_dht.readTemperature();
        if (std::abs(temperature - m_last_temperature) > 0.1)
        {
            m_mqtt.set(TemperatureID, mqtt::prop::STATE, temperature);
            if (m_last_temperature == -1)
            {
                ESP_LOGI(CONTROLS_LOG_TAG, "Temperature GPIO %d (%s): %.1f", Pin, TemperatureID.c_str(), temperature);
            }
            else
            {
                ESP_LOGI(CONTROLS_LOG_TAG, "Temperature GPIO %d (%s): %.1f -> %.1f", Pin, TemperatureID.c_str(),
                         m_last_temperature, temperature);
            }
            m_last_temperature = temperature;
        }
        float humidity = m_dht.readHumidity();
        if (std::abs(humidity - m_last_humidity) > 0.1)
        {
            m_mqtt.set(HumidityID, mqtt::prop::STATE, humidity);
            if (m_last_humidity == -1)
            {
                ESP_LOGI(CONTROLS_LOG_TAG, "Humidity GPIO %d (%s): %.1f", Pin, HumidityID.c_str(), humidity);
            }
            else
            {
                ESP_LOGI(CONTROLS_LOG_TAG, "Humidity GPIO %d (%s): %.1f -> %.1f", Pin, HumidityID.c_str(),
                         m_last_humidity, humidity);
            }
            m_last_humidity = humidity;
        }
    }

  public:
    const std::string TemperatureID;
    const std::string HumidityID;
    const std::string Name;
    const int Pin;

  private:
    mqtt::Client &m_mqtt;
    DHT m_dht;

    float m_last_temperature = -1;
    float m_last_humidity = -1;
    unsigned long m_last_update_ms = 0;
};
