///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// Simple USB client class providing two bulk endpoints to
// be used with the CMSIS DAP v2 protocol. Requests are
// buffered in a FIFO. Synchronization of USB handlers and
// the task code is done with suspend/resume. A condition
// variable would also work, but causes more overhead.
//
#ifndef USB_DAP_DEVICE_H
#define USB_DAP_DEVICE_H

#include "usb_configuration.h"
#include "usb_device_controller.h"
#include "usb_interface.h"
#include "usb_fifo.h"

#include "DAP_protocol.h"

#include "task.h"

class usb_dap_device : public task {
public:

    usb_dap_device(usb_device_controller & controller,
                   usb_configuration & configuration,
                   DAP_Protocol & dap);

    [[noreturn]] void run() override;

    // Read only version of interface descriptor
    const usb_interface &   interface_dap;

private:

    // The DAP protocol instance
    DAP_Protocol & _dap;

    // USB CMSIS DAP descriptor tree
    usb_configuration & _configuration;
    usb_interface _if_dap {_configuration};

    // USB endpoints
    usb_endpoint * _ep_dap_in  {nullptr};
    usb_endpoint * _ep_dap_out {nullptr};

    // FIFOs for receive/transmit data
    fifo<usb_buf_t, DAP_MAX_PACKET_COUNT> _received_data;
    fifo<usb_buf_t, DAP_MAX_PACKET_COUNT> _transmit_data;

    // Internal USB data buffers
    usb_buf_t _buffer_out;
    usb_buf_t _buffer_in;
};

#endif  // USB_DAP_DEVICE_H
