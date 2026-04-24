///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// Implementation of LEDs for the RPi Debug Probe.
//
#ifndef DAP_LED_RPI_DEBUG_PROBE_H
#define DAP_LED_RPI_DEBUG_PROBE_H

#include <cstdint>
#include "config.h"
#include "DAP_led_interface.h"
#include "task.h"

// Time after which UART LEDs are switched off,
// when there is no further UART activity.
#define LED_FOLLOW_UP_TIME_MS 20

class DAP_led_rpi_debug_probe : public DAP_led_interface {
public:
    DAP_led_rpi_debug_probe() {
        // Set LEDs to output
        _power_led.gpioMode(GPIO::OUTPUT);
        _uart_rx_led.gpioMode(GPIO::OUTPUT);
        _uart_tx_led.gpioMode(GPIO::OUTPUT);
        _connected_led.gpioMode(GPIO::OUTPUT);
        _running_led.gpioMode(GPIO::OUTPUT);

        // Switch off the RX/TX LEDs
        _follow_up_timer.setCallback([&] () {
            _uart_rx_led = LOW;
            _uart_tx_led = LOW;
        });
    }

    // UART LEDs handling
    void trigger_uart_tx_led() override {
        _uart_tx_led = HIGH;
        if (_follow_up_timer.isRunning()) {
            _follow_up_timer.reset();
        } else {
            _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                       TIMER::ONE_SHOT);
            _follow_up_timer.start();
        }
    }

    void trigger_uart_rx_led() override {
        _uart_rx_led = HIGH;
        if (_follow_up_timer.isRunning()) {
            _follow_up_timer.reset();
        } else {
            _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                       TIMER::ONE_SHOT);
            _follow_up_timer.start();
        }
    }

    // Switch on/off the LED signalling the
    // connection status between debugger and target.
    inline void set_connected_led(bool val) override {
        if (val) _just_connected = true;
        _connected_led = val;
    }

    // Switch on/off the LED signalling the
    // 'target running' status
    inline void set_running_led(bool val) override {
        if (_just_connected) {
            // Ignore the first switch-on from openocd...
            _just_connected = false;
        } else {
            _running_led = val;
        }
    }

    // This method will also switch on the Power LED
    // on GPIO2 - signalling successful USB connection
    void welcome() override {
        _power_led = HIGH;
        for(int i=1; i < 3; ++i) {
            bool val = (bool)(i % 2);
            _uart_rx_led = val;
            _uart_tx_led = val;
            _connected_led = val;
            _running_led   = val;
            task::sleep_ms(200);
        }
    }

private:
    gpio_rp2xxx   _power_led     {POWER_LED};
    gpio_rp2xxx   _uart_rx_led   {UART_RX_LED};
    gpio_rp2xxx   _uart_tx_led   {UART_TX_LED};
    gpio_rp2xxx   _connected_led {DAP_CONNECTED_LED};
    gpio_rp2xxx   _running_led   {DAP_RUNNING_LED};

    timer_rp2xxx  _follow_up_timer;
    bool          _just_connected {false};
};

#endif // DAP_LED_RPI_DEBUG_PROBE_H
