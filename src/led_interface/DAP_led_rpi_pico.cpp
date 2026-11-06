///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "DAP_led_rpi_pico.h"
#include "task.h"

DAP_led_rpi_pico::DAP_led_rpi_pico() {
    // Set LED to output
    _led.gpioMode(GPIO::OUTPUT);
    // This timer will blink the LED
    _blink_timer.setPeriod(50000, TIMER::PERIODIC);
    _blink_timer.setCallback([&] () {
        _led.gpioToggle();
    });
}

void DAP_led_rpi_pico::trigger_uart_tx_led() {
}

void DAP_led_rpi_pico::trigger_uart_rx_led() {
}

void DAP_led_rpi_pico::set_connected_led(bool val) {
    if (val) _just_connected = true;
    _led = val;
    _connected = val;
}

void DAP_led_rpi_pico::set_running_led(bool val) {
    if (_just_connected) {
        // Ignore the first switch-on from openocd...
        _just_connected = false;
    } else {
        if (val) {
            _blink_timer.start();
        } else {
            _blink_timer.stop();
            _led = _connected;
        }
    }
}

void DAP_led_rpi_pico::welcome() {
    for(int i=1; i < 3; ++i) {
        _led = (bool)(i % 2);
        task::sleep_ms(200);
    }
}
