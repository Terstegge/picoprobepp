// ---------------------------------------------
//           This file is part of
//      _  _   __    _   _    __    __
//     ( \/ ) /__\  ( )_( )  /__\  (  )
//      \  / /(__)\  ) _ (  /(__)\  )(__
//      (__)(__)(__)(_) (_)(__)(__)(____)
//
//     Yet Another HW Abstraction Library
//      Copyright (C) Andreas Terstegge
//      BSD Licensed (see file LICENSE)
//
// ---------------------------------------------
// Implementation of LEDs for the RP2350-Launchpad
// debug probe. The WS2812 LEDs are used in this case.
//
#ifndef DAP_LED_RP2350_LAUNCHPAD_H
#define DAP_LED_RP2350_LAUNCHPAD_H

#include <cstdint>
#include "config.h"
#include "DAP_led_interface.h"
#include "task.h"
#include "ws2812_rp2350.h"

// LED configuration
#define LED_FOLLOW_UP_TIME_MS 20

class DAP_led_rp2350_launchpad : public DAP_led_interface {
public:

    DAP_led_rp2350_launchpad() {
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

    // Switch on the LED signalling UART TX
    // activity from the target. After the
    // follow-up time, the LED is switched
    // off automatically.
    void trigger_uart_tx_led() override {
        _red = true;
        if (_follow_up_timer.isRunning()) {
            _follow_up_timer.reset();
        } else {
            _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000, TIMER::ONE_SHOT);
            _follow_up_timer.start();
        }
    }

    // Switch on the LED signalling UART RX
    // activity to the target. After the
    // follow-up time, the LED is switched
    // off automatically.
    void trigger_uart_rx_led() override {
        _green = true;
        if (_follow_up_timer.isRunning()) {
            _follow_up_timer.reset();
        } else {
            _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000, TIMER::ONE_SHOT);
            _follow_up_timer.start();
        }
    }

    // Switch on/off the LED signalling the
    // connection status between debugger and target.
    inline void set_connected_led(bool val) override {
        if (val) _just_connected = true;
        _blue = val;
    }

    // Switch on/off the LED signalling the
    // 'target running' status
    inline void set_running_led(bool val) override {
        if (_just_connected) {
            // Ignore the first switch-on from openocd...
            _just_connected = false;
        } else {
            _yellow = val;
        }
    }

    void welcome() override {
        for(int i=1; i < 3; ++i) {
            _red   = (bool)(i % 2);
            _green = (bool)(i % 2);
            _blue  = (bool)(i % 2);
            task::sleep_ms(100);
        }
    }

private:
    ws2812_rp2350 _leds {LED_RGB_GPIO, LED_RGB_COUNT};
    led_interface & _red    {_leds[0]};
    led_interface & _green  {_leds[1]};
    led_interface & _blue   {_leds[2]};
    led_interface & _yellow {_leds[3]};

    timer_rp2xxx  _follow_up_timer;

    bool _just_connected {false};
};

#endif // DAP_LED_RP2350_LAUNCHPAD_H
