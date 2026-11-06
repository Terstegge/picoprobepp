///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
/**
 * @file DAP_led_rpi_pico.h
 * @brief LED implementation for the RPi Pico and Pico2.
 *
 * There is only one (green) LED available, so UART activity is
 * not shown.
 */
#ifndef DAP_LED_RPI_PICO_H
#define DAP_LED_RPI_PICO_H

#include "config.h"
#include "DAP_led_interface.h"

/**
 * @brief LED implementation for the RPi Pico and Pico2.
 */
class DAP_led_rpi_pico : public DAP_led_interface {
public:
    /**
     * @brief Construct the RPi Pico LED handler.
     */
    DAP_led_rpi_pico();

    /**
     * @brief UART TX LED signalling is not implemented.
     */
    void trigger_uart_tx_led() override;

    /**
     * @brief UART RX LED signalling is not implemented.
     */
    void trigger_uart_rx_led() override;

    /**
     * @brief Switch on/off the LED signalling the connection status.
     *
     * The LED shows the connection status between debugger and target.
     */
    void set_connected_led(bool val) override;

    /**
     * @brief Switch on/off LED blinking for the 'target running' status.
     */
    void set_running_led(bool val) override;

    /**
     * @brief Show a 'welcome' LED pattern.
     */
    void welcome() override;

private:
    /// LED used for all status signalling.
    gpio_rp2xxx   _led{LED_GPIO};
    /// Timer blinking the LED while the target is running.
    timer_rp2xxx  _blink_timer;

    /// Flag for detecting the first setting of the 'running' state.
    bool          _just_connected {false};
    /// Last connection state, used to restore the LED after blinking.
    bool          _connected {false};
};

#endif // DAP_LED_RPI_PICO_H
