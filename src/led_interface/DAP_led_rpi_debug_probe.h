///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
/**
 * @file DAP_led_rpi_debug_probe.h
 * @brief LED implementation for the RPi Debug Probe.
 */
#ifndef DAP_LED_RPI_DEBUG_PROBE_H
#define DAP_LED_RPI_DEBUG_PROBE_H

#include "config.h"
#include "DAP_led_interface.h"

/**
 * @brief LED implementation for the RPi Debug Probe.
 */
class DAP_led_rpi_debug_probe : public DAP_led_interface {
public:
    /**
     * @brief Construct the RPi Debug Probe LED handler.
     */
    DAP_led_rpi_debug_probe();

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
     *
     * This method will also switch on the Power LED on GPIO2,
     * signalling a successful USB connection.
     */
    void welcome() override;

private:
    /// Power LED signalling a successful USB connection.
    gpio_rp2xxx   _power_led     {POWER_LED};
    /// LED signalling UART RX activity.
    gpio_rp2xxx   _uart_rx_led   {UART_RX_LED};
    /// LED signalling UART TX activity.
    gpio_rp2xxx   _uart_tx_led   {UART_TX_LED};
    /// LED signalling the DAP connection status.
    gpio_rp2xxx   _connected_led {DAP_CONNECTED_LED};
    /// LED signalling the 'target running' status.
    gpio_rp2xxx   _running_led   {DAP_RUNNING_LED};

    /// Timer switching off the UART activity LEDs.
    timer_rp2xxx  _follow_up_timer;

    /// Flag for detecting the first setting of the 'running' state.
    bool          _just_connected {false};
};

#endif // DAP_LED_RPI_DEBUG_PROBE_H
