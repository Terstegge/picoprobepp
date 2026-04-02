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
//
// Simple USB CDC ACM client class, which controls
// a real UART (provided via the uart_interface).
// Synchronization between the data handlers and
// the task code is implemented with suspend/resume.
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
