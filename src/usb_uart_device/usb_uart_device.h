///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#ifndef USB_UART_DEVICE_H
#define USB_UART_DEVICE_H

#include <cassert>
#include <functional>
#include "config.h"
#include "FIFO.h"
#include "task.h"
#include "uart_interface.h"
#include "usb_cdc_acm_device.h"
#include "usb_device_controller.h"

class usb_uart_device : public usb_cdc_acm_device, public task {
public:

    usb_uart_device(usb_device_controller &ctrl,
                    usb_configuration &conf,
                    uart_interface &uart);

    // Notify the DCD/DSR state
    void set_dcd_dtr(bool dcd, bool dsr);

    // Task method
    [[noreturn]] void run() override;

    static std::function<void()> uart_tx_cb;
    static std::function<void()> uart_rx_cb;

private:
    uart_interface &_uart;
    bool            _dtr {false};
    FIFO<uint8_t>   _tx_buffer{UART_TARGET_BUFFER_SIZE};
    uint8_t         _rx_buffer[UART_TARGET_BUFFER_SIZE] {0};
    timer_rp2xxx    _host_active_timer;
    bool            _host_active {false};
};

#endif // USB_UART_DEVICE_H
