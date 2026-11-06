///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "DAP_led_rp2xxx_launchpad_probe.h"
#include "task.h"

#include "RP2040.h"
using namespace _IO_BANK0_;
using namespace _PWM_;

// Time after which the UART LEDs are switched off,
// when there is no further UART activity.
#define LED_FOLLOW_UP_TIME_MS 20
// Period of the blue LED blinking while the target is running.
#define BLUE_BLINK_PERIOD_MS  50
// Brightness values are uint16_t.
// 0 is OFF, 65535 max brightness!
#define RED_BRIGHTNESS        15000
#define GREEN_BRIGHTNESS      10000
#define BLUE_BRIGHTNESS       35000
#define BLUE_DIM_BRIGHTNESS   10000

DAP_led_rp2xxx_launchpad_probe::DAP_led_rp2xxx_launchpad_probe() {
    // Set GPIOs to output
    _red.gpioMode  (GPIO::OUTPUT | GPIO::INIT_LOW);
    _green.gpioMode(GPIO::OUTPUT | GPIO::INIT_LOW);
    _blue.gpioMode (GPIO::OUTPUT | GPIO::INIT_LOW);

    // Switch off the RX/TX LEDs
    _follow_up_timer.setCallback([&] () {
        set_red(false);
        set_green(false);
    });

    // Blink the blue LED
    _blue_blink_timer.setCallback([&]() {
        // This code will only blink the blue LED
        // while the blue LED is on!
        if (PWM.CH1_CC.A == BLUE_BRIGHTNESS) {
            PWM.CH1_CC.A  = BLUE_DIM_BRIGHTNESS;
        } else {
            if (PWM.CH1_CC.A == BLUE_DIM_BRIGHTNESS) {
                PWM.CH1_CC.A  = BLUE_BRIGHTNESS;
            }
        }
    });
    _blue_blink_timer.setPeriod(BLUE_BLINK_PERIOD_MS * 1000,
                                TIMER::PERIODIC);

    // Set up PWM for all LEDs
    _red.  setSEL(GPIO_CTRL_FUNCSEL__pwm);
    _green.setSEL(GPIO_CTRL_FUNCSEL__pwm);
    _blue. setSEL(GPIO_CTRL_FUNCSEL__pwm);

    uint32_t div = CLK_SYS / 1000 / 4096;
    PWM.CH1_DIV.INT  = div / 16;
    PWM.CH1_DIV.FRAC = div % 16;
    PWM.CH1_TOP      = 65535;
    PWM.CH1_CC.A     = 0; // BLUE OFF
    PWM.CH1_CC.B     = 0; // GREEN OFF
    PWM.CH1_CSR.EN   = 1;
    PWM.CH2_DIV.INT  = div / 16;
    PWM.CH2_DIV.FRAC = div % 16;
    PWM.CH2_TOP      = 65535;
    PWM.CH2_CC.A     = 0; // RED OFF
    PWM.CH2_CSR.EN   = 1;
}

void DAP_led_rp2xxx_launchpad_probe::trigger_uart_tx_led() {
    set_red(true);
    if (_follow_up_timer.isRunning()) {
        _follow_up_timer.reset();
    } else {
        _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                   TIMER::ONE_SHOT);
        _follow_up_timer.start();
    }
}

void DAP_led_rp2xxx_launchpad_probe::trigger_uart_rx_led() {
    set_green(true);
    if (_follow_up_timer.isRunning()) {
        _follow_up_timer.reset();
    } else {
        _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                   TIMER::ONE_SHOT);
        _follow_up_timer.start();
    }
}

void DAP_led_rp2xxx_launchpad_probe::set_connected_led(bool val) {
    if (val) _just_connected = true;
    set_blue(val);
}

void DAP_led_rp2xxx_launchpad_probe::set_running_led(bool val) {
    if (_just_connected) {
        // Ignore the first switch-on from openocd...
        _just_connected = false;
    } else {
        if (val) {
            _blue_blink_timer.start();
        } else {
            _blue_blink_timer.stop();
            if (PWM.CH1_CC.A == BLUE_DIM_BRIGHTNESS) {
                PWM.CH1_CC.A  = BLUE_BRIGHTNESS;
            }
        }
    }
}

void DAP_led_rp2xxx_launchpad_probe::welcome() {
    // Blink once. Could be adjusted in later firmware versions.
    for(int i=1; i<=2; ++i) {
        set_red  (i & 1);
        set_green(i & 1);
        set_blue (i & 1);
        task::sleep_ms(200);
    }
}

void DAP_led_rp2xxx_launchpad_probe::set_red(bool b) {
    PWM.CH2_CC.A = b ? RED_BRIGHTNESS : 0;
}

void DAP_led_rp2xxx_launchpad_probe::set_green(bool b) {
    PWM.CH1_CC.B = b ? GREEN_BRIGHTNESS : 0;
}

void DAP_led_rp2xxx_launchpad_probe::set_blue(bool b) {
    PWM.CH1_CC.A = b ? BLUE_BRIGHTNESS : 0;
}
