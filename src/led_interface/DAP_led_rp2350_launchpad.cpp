///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "DAP_led_rp2350_launchpad.h"
#include "task.h"

// Time after which the UART LEDs are switched off,
// when there is no further UART activity.
#define LED_FOLLOW_UP_TIME_MS 20

DAP_led_rp2350_launchpad::DAP_led_rp2350_launchpad() {
    // Set the colors
    _leds[0].set_on_color(0x100000);
    _leds[1].set_on_color(0x001000);
    _leds[2].set_on_color(0x000030);
    _leds[3].set_on_color(0x101000);

    // The timer will switch off the RX/TX LEDs
    _follow_up_timer.setCallback([&] () {
        _red   = false;
        _green = false;
    });
}

void DAP_led_rp2350_launchpad::trigger_uart_tx_led() {
    _red = true;
    if (_follow_up_timer.isRunning()) {
        _follow_up_timer.reset();
    } else {
        _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                   TIMER::ONE_SHOT);
        _follow_up_timer.start();
    }
}

void DAP_led_rp2350_launchpad::trigger_uart_rx_led() {
    _green = true;
    if (_follow_up_timer.isRunning()) {
        _follow_up_timer.reset();
    } else {
        _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                   TIMER::ONE_SHOT);
        _follow_up_timer.start();
    }
}

void DAP_led_rp2350_launchpad::set_connected_led(bool val) {
    if (val) _just_connected = true;
    _blue = val;
}

void DAP_led_rp2350_launchpad::set_running_led(bool val) {
    if (_just_connected) {
        // Ignore the first switch-on from openocd...
        _just_connected = false;
    } else {
        _yellow = val;
    }
}

void DAP_led_rp2350_launchpad::welcome() {
    for(int i=1; i < 3; ++i) {
        _red   = (bool)(i % 2);
        _green = (bool)(i % 2);
        _blue  = (bool)(i % 2);
        task::sleep_ms(200);
    }
}
