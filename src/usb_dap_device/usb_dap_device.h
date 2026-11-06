///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
/**
 * @file usb_dap_device.h
 * @brief Simple USB client class for the CMSIS DAP v2 protocol.
 *
 * Provides two bulk endpoints to be used with the CMSIS DAP v2
 * protocol. Requests are buffered in a FIFO. Synchronization of
 * USB handlers and the task code is done with suspend/resume. A
 * condition variable would also work, but causes more overhead.
 */
#ifndef USB_DAP_DEVICE_H
#define USB_DAP_DEVICE_H

#include "usb_configuration.h"
#include "usb_device_controller.h"
#include "usb_interface.h"
#include "usb_fifo.h"

#include "DAP_protocol.h"
#include "task.h"

/**
 * @brief USB client task providing the CMSIS DAP v2 bulk interface.
 */
class usb_dap_device : public task {
public:

    /**
     * @brief Construct the CMSIS DAP USB device task.
     * @param controller USB device controller used to create the endpoints.
     * @param configuration USB configuration the DAP interface is added to.
     * @param dap DAP protocol instance used to process requests.
     */
    usb_dap_device(usb_device_controller & controller,
                   usb_configuration & configuration,
                   DAP_Protocol & dap);

    /**
     * @brief The task code.
     */
    [[noreturn]] void run() override;

    /**
     * @brief Read only version of the interface descriptor.
     */
    const usb_interface & interface_dap;

private:

    /**
     * @brief The DAP protocol instance.
     */
    DAP_Protocol & _dap;

    /**
     * @brief USB CMSIS DAP descriptor tree.
     */
    usb_configuration & _configuration;
    usb_interface _if_dap {_configuration};

    /**
     * @brief USB endpoints.
     */
    usb_endpoint * _ep_dap_in  {nullptr};
    usb_endpoint * _ep_dap_out {nullptr};

    /**
     * @brief FIFOs for receive/transmit data.
     */
    fifo<usb_buf_t, DAP_MAX_PACKET_COUNT> _received_data;
    fifo<usb_buf_t, DAP_MAX_PACKET_COUNT> _transmit_data;

    /**
     * @brief Internal USB data buffers.
     */
    usb_buf_t _buffer_out;
    usb_buf_t _buffer_in;
};

#endif  // USB_DAP_DEVICE_H
