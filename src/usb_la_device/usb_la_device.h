///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// This class implements the USB interface for the logic analyzer.
// It implements a specific CDC ACM Device, which provides special
// methods for transferring bigger blocks of data (the samples) as
// well as methods to send/receive ASCII characters for the control
// protocol.
//
#ifndef USB_LA_DEVICE_H
#define USB_LA_DEVICE_H

#include "usb_cdc_structs.h"
#include "usb_configuration.h"
#include "usb_device_controller.h"
#include "usb_interface.h"
#include "usb_interface_association.h"
#include "usb_fd_header.h"
#include "usb_fd_call_mgmt.h"
#include "usb_fd_acm.h"
#include "usb_fd_union.h"
#include "usb_fifo.h"
#include "task.h"

class usb_la_device final : public task {
public:
    usb_la_device(usb_device_controller & controller,
                  usb_configuration & configuration);

    /**
     * @brief Get a single ASCII character from the host.
     * The method will block until a character is received.
     * @return The ASCII received character
     */
    char getc();

    /**
     * @brief Return the number of available chrarters to
     * be received. Use getc() to actually get those.
     * @return Number of available characters.
     */
    uint16_t available();

    /**
     * @brief Write a character string to the host.
     * @param str The string to be sent. Strings with
     * length 0 as well as nullptr will be ignored.
     */
    void write_response(const char *str) const;

    /**
     * @brief Write a single character to the host
     * @param c The character to write.
     */
    void write_response(char c) const;

    /**
     * @brief Queue a block of data to be sent to the host.
     * The data array is not copied - only the pointer to
     * the data and the length are stored in a small FIFO.
     * @param data Pointer to the byte array to be sent.
     * @param len The length of the data array
     * @return false if method didn't succeed, true otherwise.
     */
    bool queue_block(uint8_t* data, size_t len);

    /**
     * @brief Check if the sending process via USB is still busy
     * @return true if tx is busy, false if tx idle
     */
    bool is_tx_busy() {
        return (_transmit_queue.available_get() != 0)
                || (_ep_data_in->is_active());
    }

    /**
     * @brief The task code
     */
    [[noreturn]] void run() override;

private:

    // Standard CDC-ACM descriptor tree
    usb_configuration &         _configuration;
    usb_interface_association   _assoc        {_configuration};
    usb_interface               _if_ctrl      {_assoc};
    usb_interface               _if_data      {_assoc};
    usb_fd_header               _header_fd    {_if_ctrl};
    usb_fd_call_mgmt            _call_mgmt_fd {_if_ctrl};
    usb_fd_acm                  _acm_fd       {_if_ctrl};
    usb_fd_union                _union_fd     {_if_ctrl};

    // USB endpoints
    usb_endpoint *  _ep_ctrl_in   {nullptr};
    usb_endpoint *  _ep_data_in   {nullptr};
    usb_endpoint *  _ep_data_out  {nullptr};

    // Line Coding Dummy object
    CDC::line_coding_t _line_coding;

    // Small FIFO for the received characters from the host
    fifo<uint8_t, 128> _rx_fifo;
    // USB output buffer, used when data ist sent to the host
    uint8_t _buffer_out[TUPP_DEFAULT_PAKET_SIZE] {};

    // Data struct to be used with the FIFO below. One entry
    // will store a pointer to the data and the size in bytes.
    struct la_block_t {
        uint8_t * data {nullptr};
        size_t    len  {0};
    };

    // Queue for send requests. Only pointers to
    // the data are stored, no copying of data!
    fifo<la_block_t, 4> _transmit_queue;
};

#endif // USB_LA_DEVICE_H
