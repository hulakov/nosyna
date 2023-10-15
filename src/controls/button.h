
#pragma once

#include "common.h"

#include <Arduino.h>

#include <functional>

class Button
{
  public:
    Button(int pin) : Pin(pin)
    {
    }

    void loop()
    {
        auto value = digitalRead(Pin);
        if (value != m_pin_value)
        {
            ESP_LOGD(CONTROLS_LOG_TAG, "Button GPIO %d: %d -> %d", Pin, m_pin_value, value);
            if (m_pin_value != -1 && value == 0 && m_on_click != nullptr)
            {
                ESP_LOGI(CONTROLS_LOG_TAG, "Button GPIO %d click", Pin);
                m_on_click();
            }
            m_pin_value = value;
        }
    }

    void set_on_click(std::function<void()> on_click)
    {
        m_on_click = on_click;
    }

    const int Pin;

  private:
    int m_pin_value = -1;
    std::function<void()> m_on_click;
};