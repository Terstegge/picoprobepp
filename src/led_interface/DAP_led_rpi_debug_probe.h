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

#define LED_FOLLOW_UP_TIME_MS 20

class DAP_led_rpi_debug_probe : public DAP_led_interface {
public:
    DAP_led_rpi_debug_probe() {
        // Set LEDs to output
        _power.gpioMode(GPIO::OUTPUT | GPIO::INIT_LOW);
        _uart_rx.gpioMode(GPIO::OUTPUT);
        _uart_tx.gpioMode(GPIO::OUTPUT);
        _dap_connected.gpioMode(GPIO::OUTPUT);
        _dap_running.gpioMode(GPIO::OUTPUT);

        // Switch off the RX/TX LEDs
        _follow_up_timer.setCallback([&] () {
            _uart_rx = 0;
            _uart_tx = 0;
        });
    }

    // UART LEDs handling
    void trigger_uart_tx_led() override {
        _uart_tx = true;
        if (_follow_up_timer.isRunning()) {
            _follow_up_timer.reset();
        } else {
            _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                       TIMER::ONE_SHOT);
            _follow_up_timer.start();
        }
    }

    void trigger_uart_rx_led() override {
        _uart_rx = true;
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
        _dap_connected = val;
    }

    // Switch on/off the LED signalling the
    // 'target running' status
    inline void set_running_led(bool val) override {
        if (_just_connected) {
            // Ignore the first switch-on from openocd...
            _just_connected = false;
        } else {
            _dap_running = val;
        }
    }

    // This method will also switch on the Power LED
    // on GPIO2 -signalling successful USB connection
    void welcome() override {
        _power = HIGH;
        for(int i=1; i < 3; ++i) {
            bool val = (bool)(i % 2);
            _uart_rx = val;
            _uart_tx = val;
            _dap_connected = val;
            _dap_running   = val;
            task::sleep_ms(200);
        }
    }

private:
    gpio_rp2xxx   _power         {POWER_LED};
    gpio_rp2xxx   _uart_rx       {UART_RX_LED};
    gpio_rp2xxx   _uart_tx       {UART_TX_LED};
    gpio_rp2xxx   _dap_connected {DAP_CONNECTED_LED};
    gpio_rp2xxx   _dap_running   {DAP_RUNNING_LED};

    timer_rp2xxx  _follow_up_timer;
    bool          _just_connected {false};
};

#endif // DAP_LED_RPI_DEBUG_PROBE_H
