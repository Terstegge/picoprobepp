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
// Implementation of LEDs for the RP2040-based debug
// probe on the RP2040 or RP2350 Launchpad boards.
//
#ifndef DAP_LED_RP2XX0_LP_PROBE_H
#define DAP_LED_RP2XX0_LP_PROBE_H

#include <cstdint>
#include "config.h"
#include "task.h"
#include "DAP_led_interface.h"

#include "RP2040.h"
using namespace _IO_BANK0_;
using namespace _PWM_;

// LED configuration
#define LED_FOLLOW_UP_TIME_MS 20
#define BLUE_BLINK_PERIOD_MS  50
// Brigtness values are uint16_t.
// 0 is OFF, 65535 max brightness!
#define RED_BRIGHTNESS        15000
#define GREEN_BRIGHTNESS      10000
#define BLUE_BRIGHTNESS       35000
#define BLUE_DIM_BRIGHTNESS   10000

class DAP_led_rp2xxx_launchpad_probe : public DAP_led_interface {
public:
    DAP_led_rp2xxx_launchpad_probe() {
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

        // Set up PWM for blue LED
        _red.  setSEL(_IO_BANK0_::GPIO_CTRL_FUNCSEL__pwm);
        _green.setSEL(_IO_BANK0_::GPIO_CTRL_FUNCSEL__pwm);
        _blue. setSEL(_IO_BANK0_::GPIO_CTRL_FUNCSEL__pwm);

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

    // Switch on the LED signalling UART TX
    // activity from the target. After the
    // follow-up time, the LED is switched
    // off automatically.
    void trigger_uart_tx_led() override {
        set_red(true);
        if (_follow_up_timer.isRunning()) {
            _follow_up_timer.reset();
        } else {
            _follow_up_timer.setPeriod(LED_FOLLOW_UP_TIME_MS * 1000,
                                       TIMER::ONE_SHOT);
            _follow_up_timer.start();
        }
    }

    // Switch on the LED signalling UART RX
    // activity to the target. After the
    // follow-up time, the LED is switched
    // off automatically.
    void trigger_uart_rx_led() override {
        set_green(true);
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
        set_blue(val);
    }

    // Switch on/off the LED signalling the
    // 'target running' status
    inline void set_running_led(bool val) override {
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

    void welcome() override {
        set_red(true);
        set_green(true);
        set_blue(true);
        task::sleep_ms(200);
        set_red(false);
        set_green(false);
        set_blue(false);
    }

private:

    static inline void set_red(bool b) {
        PWM.CH2_CC.A = b ? RED_BRIGHTNESS : 0; // RED ON
    }

    static inline void set_green(bool b) {
        PWM.CH1_CC.B = b ? GREEN_BRIGHTNESS : 0; // RED ON
    }

    static inline void set_blue(bool b) {
        PWM.CH1_CC.A = b ? BLUE_BRIGHTNESS : 0; // RED ON
    }

    gpio_rp2xxx     _red   {LED_RED_GPIO};   // UART TX (data from target)
    gpio_rp2xxx     _green {LED_GREEN_GPIO}; // UART RX (data to target)
    gpio_rp2xxx     _blue  {LED_BLUE_GPIO};

    timer_rp2xxx    _follow_up_timer;
    timer_rp2xxx    _blue_blink_timer;

    bool            _just_connected {false};
};

#endif // DAP_LED_RP2XX0_LP_PROBE_H
