///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "DAP_led_rpi_debug_probe.h"
#include "task.h"

// Time after which the UART LEDs are switched off,
// when there is no further UART activity.
#define LED_FOLLOW_UP_TIME_MS 20

DAP_led_rpi_debug_probe::DAP_led_rpi_debug_probe() {
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

void DAP_led_rpi_debug_probe::trigger_uart_tx_led() {
    _uart_tx_led = HIGH;
    if (_follow_up_timer.isRunning()) {
        _follow_up_timer.reset();
    } else {
        _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                   TIMER::ONE_SHOT);
        _follow_up_timer.start();
    }
}

void DAP_led_rpi_debug_probe::trigger_uart_rx_led() {
    _uart_rx_led = HIGH;
    if (_follow_up_timer.isRunning()) {
        _follow_up_timer.reset();
    } else {
        _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                   TIMER::ONE_SHOT);
        _follow_up_timer.start();
    }
}

void DAP_led_rpi_debug_probe::set_connected_led(bool val) {
    if (val) _just_connected = true;
    _connected_led = val;
}

void DAP_led_rpi_debug_probe::set_running_led(bool val) {
    if (_just_connected) {
        // Ignore the first switch-on from openocd...
        _just_connected = false;
    } else {
        _running_led = val;
    }
}

void DAP_led_rpi_debug_probe::welcome() {
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
