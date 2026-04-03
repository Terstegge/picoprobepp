///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// Implementation of LEDs for the RPi Pico. There is only one
// green LED available, so UART activity is not shown.
//
#ifndef DAP_LED_RPI_PICO_H
#define DAP_LED_RPI_PICO_H

#include <cstdint>
#include "config.h"
#include "DAP_led_interface.h"
#include "task.h"

class DAP_led_rpi_pico : public DAP_led_interface {
public:
    DAP_led_rpi_pico() {
        // Set LED to output
        _green.gpioMode(GPIO::OUTPUT);
        // The timer will blink the green LED
        _blink_timer.setPeriod(50000, TIMER::PERIODIC);
        _blink_timer.setCallback([&] () {
            _green.gpioToggle();
        });
    }

    // UART signalling not implemented
    void trigger_uart_tx_led() override { }
    void trigger_uart_rx_led() override { }

    // Switch on/off the LED signalling the
    // connection status between debugger and target.
    inline void set_connected_led(bool val) override {
        if (val) _just_connected = true;
        _green = val;
        _connected = val;
    }

    // Switch on/off the LED signalling the
    // 'target running' status
    inline void set_running_led(bool val) override {
        if (_just_connected) {
            // Ignore the first switch-on from openocd...
            _just_connected = false;
        } else {
            if (val) {
                _blink_timer.start();
            } else {
                _blink_timer.stop();
                _green = _connected;
            }
        }
    }

    void welcome() override {
        for(int i=1; i < 3; ++i) {
            _green = (bool)(i % 2);
            task::sleep_ms(200);
        }
    }

private:
    gpio_rp2xxx   _green{LED_GREEN_GPIO};
    timer_rp2xxx  _blink_timer;
    bool          _just_connected {false};
    bool          _connected {false};
};

#endif // DAP_LED_RPI_PICO_H
