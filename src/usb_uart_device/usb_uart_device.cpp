///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include <cassert>
#include "usb_uart_device.h"

// Mark host as inactive after 500ms
#define HOST_ACTIVE_TIMEOUT_US  5000000

std::function<void()> usb_uart_device::uart_tx_cb;
std::function<void()> usb_uart_device::uart_rx_cb;

usb_uart_device::usb_uart_device(usb_device_controller &ctrl,
                                 usb_configuration &conf,
                                 uart_interface &uart)
    : usb_cdc_acm_device(ctrl, conf), task("UART ACM task"), _uart(uart)
{
    // Disable the UART FIFOs because we have strange problems with them:
    // Testing with a direct UART loopback, the RX interrupts will not be
    // generated when the UART has a full TX FIFO and is sending data all
    // the time. Even polling for received characters does not work.
    // Needs further investigation...
    _uart.enableFIFO(false);

    line_coding_handler = [&](const CDC::line_coding_t &lineCoding) {
        // Handle line code changes
        uart_mode_t mode = 0;
        switch ((int) lineCoding.bDataBits) {
            case 7:
                mode |= UART::BITS_7;
                break;
            case 8:
                mode |= UART::BITS_8;
                break;
            default:
                assert("Wrong number of bits!!" && false);
        }
        switch ((int) lineCoding.bParityType) {
            case (int) CDC::bParityType_t::PARITY_NONE:
                mode |= UART::NO_PARITY;
                break;
            case (int) CDC::bParityType_t::PARITY_EVEN:
                mode |= UART::EVEN_PARITY;
                break;
            case (int) CDC::bParityType_t::PARITY_ODD:
                mode |= UART::ODD_PARITY;
                break;
            default:
                assert("Wrong parity!" && false);
        }
        switch ((int) lineCoding.bCharFormat) {
            case (int) CDC::bCharFormat_t::STOP_BITS_1:
                mode |= UART::STOPBITS_1;
                break;
            case (int) CDC::bCharFormat_t::STOP_BITS_2:
                mode |= UART::STOPBITS_2;
                break;
            default:
                assert("Wrong stop bits!" && false);
        }
        _uart.uartMode(mode);
        _uart.setBaudrate(lineCoding.dwDTERate);
    };

    control_line_handler = ([&](bool dtr, bool rts) {
        // The DTR control line gives us a hint if there
        // is some application consuming the data on host side
        _dtr = dtr;
        // Set signals on hardware level
        _uart.setDTR(dtr);
        _uart.setRTS(rts);
    });

    break_handler = ([&](uint16_t millis) {
        _uart.sendBreak(millis);
    });

    received_handler = ([&]() {
        resume();
    });

    _uart.uartAttachIrq([&](char c) {
        // When DTR is low and host did not send any data recently
        // there might not be any consumer on host side. So simply
        // throw away data in this case.
        if (_tx_buffer.available_put() > 0 && (_dtr || _host_active)) {
            _tx_buffer.put(c);
            resume();
        }
    });

    // Set up timer for detecting host activity
    _host_active_timer.setPeriod(HOST_ACTIVE_TIMEOUT_US, TIMER::ONE_SHOT);
    _host_active_timer.setCallback([&] () {
        _host_active = false;
    });
}

void usb_uart_device::set_dcd_dtr(bool dcd, bool dsr) {
    CDC::bmUartState_t uart_state;
    uart_state.bRxCarrier_DCD = dcd;
    uart_state.bTxCarrier_DSR = dsr;
    notify_serial_state( uart_state );
}

void usb_uart_device::run() {
    while (true) {
        // Prevent this task from being suspended too early.
        // When a new characters arrive within 10ms, the task
        // stays in the READY state, and we do not waste time
        // in other tasks.
        uint64_t until = millis() + 10;
        while (millis() < until) {
            if (available() || _tx_buffer.available_get()) break;
        }
        if (!available() && !_tx_buffer.available_get()) {
            suspend();
            yield();
        }

        // Try to read data from USB host
        size_t len = read(_rx_buffer, UART_TARGET_BUFFER_SIZE);
        if (len) {
            // Trigger or reset the timer for the _host_active flag
            if (_host_active_timer.isRunning()) {
                _host_active_timer.reset();
            } else {
                _host_active = true;
                _host_active_timer.start();
            }
            // Call user callback handler
            if (uart_rx_cb) uart_rx_cb();
            // Forward data to our HW UART
            for(size_t i=0; i < len; ++i) {
                _uart.putc(_rx_buffer[i]);
            }
        }
        // Try to read data from UART
        len = _tx_buffer.available_get();
        // Only forward data to host if host is
        // active or some application running on
        // host side (DTR active).
        if (len && (_dtr || _host_active)) {
            if (uart_tx_cb) uart_tx_cb();
            for(size_t i=0; i < len; ++i) {
                _tx_buffer.get(_rx_buffer[i]);
            }
            uint32_t written = 0;
            while (written != len) {
                written += write(_rx_buffer + written, len - written);
            }
        }
    }
}
