///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
/**
 * @file DAP_led_rp2xxx_launchpad_probe.h
 * @brief LED implementation for the RP2xxx Launchpad probe.
 *
 * Implementation of LEDs for the RP2040-based debug probe on the
 * RP2040 or RP2350 Launchpad boards. This driver uses PWM control
 * for all LEDs, so the brightness can be adjusted as needed (it was
 * too high because of too small resistors in the HW design :)).
 * This embedded debug probe does not have a separate LED for the
 * 'running' state, so instead the blue LED (signalling the DAP
 * connection status) will be blinking when the target is running.
 * UART TX/RX LEDs are handled by a timer, which will switch off
 * those LEDs after a specific follow-up time.
 */
#ifndef DAP_LED_RP2XX0_LP_PROBE_H
#define DAP_LED_RP2XX0_LP_PROBE_H

#include "config.h"
#include "DAP_led_interface.h"

/**
 * @brief LED implementation for the RP2xxx Launchpad probe.
 */
class DAP_led_rp2xxx_launchpad_probe : public DAP_led_interface {
public:
    /**
     * @brief Construct the RP2xxx Launchpad probe LED handler.
     */
    DAP_led_rp2xxx_launchpad_probe();

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
     * @brief Switch on/off the LED blinking for the 'target running' status.
     */
    void set_running_led(bool val) override;

    /**
     * @brief Show a 'welcome' LED pattern.
     */
    void welcome() override;

private:
    /// Switch the red UART TX LED on/off.
    static void set_red(bool b);
    /// Switch the green UART RX LED on/off.
    static void set_green(bool b);
    /// Switch the blue DAP connection LED on/off.
    static void set_blue(bool b);

    /// GPIO for the red UART TX LED.
    gpio_rp2xxx  _red   {LED_RED_GPIO};
    /// GPIO for the green UART RX LED.
    gpio_rp2xxx  _green {LED_GREEN_GPIO};
    /// GPIO for the blue DAP connection LED.
    gpio_rp2xxx  _blue  {LED_BLUE_GPIO};

    /// Timer switching off the UART activity LEDs.
    timer_rp2xxx _follow_up_timer;
    /// Timer blinking the blue LED while the target is running.
    timer_rp2xxx _blue_blink_timer;

    /// Flag for detecting the first setting of the 'running' state.
    bool _just_connected {false};
};

#endif // DAP_LED_RP2XX0_LP_PROBE_H
