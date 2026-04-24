///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// Wrapper class to convert an usb_cdc_acm_device into
// an uart_data_interface, so it e.g. can be used for
// posix_io character input/output.
//
#ifndef USB_CDC_ACM_ADAPTER_H
#define USB_CDC_ACM_ADAPTER_H

#include <cstdint>
#include "usb_cdc_acm_device.h"
#include "uart_data_interface.h"

class usb_cdc_acm_adapter : public usb_cdc_acm_device, public uart_data_interface {
public:

    usb_cdc_acm_adapter(usb_device_controller & controller, usb_configuration & conf)
    : usb_cdc_acm_device(controller, conf) {
    }

    inline bool available() override {
        return usb_cdc_acm_device::available() > 0;
    }

    inline char getc() override {
        uint8_t c {0};
        usb_cdc_acm_device::read(&c, 1);
        return c;
    }

    inline void putc(char c) override {
        usb_cdc_acm_device::write((uint8_t *)&c, 1);
    }

    // Interrupt handling not supported
    void uartAttachIrq (function<void(char)> ) override { };
    void uartDetachIrq () override { };
    void uartEnableIrq () override { };
    void uartDisableIrq() override { };
};

#endif // USB_CDC_ACM_ADAPTER_H
