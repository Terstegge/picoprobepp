///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
/**
 * @file DAP_led_rp2350_launchpad.h
 * @brief LED implementation for the RP2350 Launchpad based debug probe.
 *
 * This implementation is used during development. The WS2812 LEDs
 * are used to signal the DAP and UART states.
 */
#ifndef DAP_LED_RP2350_LAUNCHPAD_H
#define DAP_LED_RP2350_LAUNCHPAD_H

#include "config.h"
#include "DAP_led_interface.h"
#include "ws2812_rp2350.h"

/**
 * @brief LED implementation for the RP2350 Launchpad based debug probe.
 */
class DAP_led_rp2350_launchpad : public DAP_led_interface {
public:
    /**
     * @brief Construct the RP2350 Launchpad LED handler.
     */
    DAP_led_rp2350_launchpad();

    /**
     * @brief Switch on the LED signalling UART TX activity from the target.
     *
     * After the follow-up time, the LED is switched off automatically.
     */
    void trigger_uart_tx_led() override;

    /**
     * @brief Switch on the LED signalling UART RX activity to the target.
     *
     * After the follow-up time, the LED is switched off automatically.
     */
    void trigger_uart_rx_led() override;

    /**
     * @brief Switch on/off the LED signalling the connection status.
     *
     * The LED shows the connection status between debugger and target.
     */
    void set_connected_led(bool val) override;

    /**
     * @brief Switch on/off the LED signalling the 'target running' status.
     */
    void set_running_led(bool val) override;

    /**
     * @brief Show a 'welcome' LED pattern.
     */
    void welcome() override;

private:
    /// WS2812 LED strip used for the status LEDs.
    ws2812_rp2350 _leds {LED_RGB_GPIO, LED_RGB_COUNT};

    /// LED signalling UART TX activity.
    led_interface & _red    {_leds[0]};
    /// LED signalling UART RX activity.
    led_interface & _green  {_leds[1]};
    /// LED signalling the DAP connection status.
    led_interface & _blue   {_leds[2]};
    /// LED signalling the 'target running' status.
    led_interface & _yellow {_leds[3]};

    /// Timer switching off the UART activity LEDs.
    timer_rp2xxx  _follow_up_timer;

    /// Flag for detecting the first setting of the 'running' state.
    bool _just_connected {false};
};

#endif // DAP_LED_RP2350_LAUNCHPAD_H
