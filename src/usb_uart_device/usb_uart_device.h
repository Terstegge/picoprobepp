///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// This class connects a CDC ACM Device with an UART interface.
// Data coming from the USB device has to be forwarded to the
// UART object and vice versa, using buffers in both directions.
// Additionally, the control signals have to be evaluated and
// forwarded if needed.
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

    // The constructor gets USB information and the UART device
    usb_uart_device(usb_device_controller &ctrl,
                    usb_configuration &conf,
                    uart_interface &uart);

    // Notify the DCD/DSR state
    void set_dcd_dtr(bool dcd, bool dsr);

    // Task method
    [[noreturn]] void run() override;

    // Callback methods, which will be called on TX/RX activity.
    // This can be used e.g. for LED signalling.
    static std::function<void()> uart_tx_cb;
    static std::function<void()> uart_rx_cb;

private:
    uart_interface & _uart;  // The UART object to connect to
    bool             _dtr {false};
    FIFO<uint8_t>    _tx_buffer{UART_TARGET_BUFFER_SIZE};
    uint8_t          _rx_buffer[UART_TARGET_BUFFER_SIZE] {0};
    timer_rp2xxx     _host_active_timer;
    bool             _host_active {false};
};

#endif // USB_UART_DEVICE_H
