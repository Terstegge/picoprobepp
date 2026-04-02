///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "usb_dap_device.h"
#include "usb_structs.h"

using namespace TUPP;
using enum TUPP::bInterfaceClass_t;
using enum TUPP::bInterfaceSubClass_t;
using enum TUPP::bInterfaceProtocol_t;
using enum TUPP::ep_attributes_t;
using enum TUPP::direction_t;

// The DAP protocol is packet-oriented, meaning it follows a strict
// send-receive pattern. Therefore, USB flow control using STALL is
// not necessary here. To improve the USB throughput, the debugger
// can configure the DAP_MAX_PACKET_COUNT, which means that the
// debugger can store this amount of USB packet without processing
// them. The host can send up to this number of DAP request packets
// without waiting for the response.

usb_dap_device::usb_dap_device( usb_device_controller & controller,
                                usb_configuration & configuration,
                                DAP_Protocol & dap)
: task("CMSIS DAP task"),
  interface_dap(_if_dap),
  _dap(dap), _configuration(configuration)
{
    TUPP_LOG(LOG_DEBUG, "usb_dap_device() @%x", this);

    // USB interface descriptors config
    _if_dap.set_bInterfaceClass    (IF_CLASS_VENDOR_SPECIFIC);
    _if_dap.set_bInterfaceSubClass (IF_SUBCLASS_NONE);
    _if_dap.set_bInterfaceProtocol (IF_PROTOCOL_NONE);
    _if_dap.set_InterfaceName      ("CMSIS-DAP v2 Interface");

    // USB endpoints
    _ep_dap_out = controller.create_endpoint(_if_dap, DIR_OUT, TRANS_BULK);
    _ep_dap_in  = controller.create_endpoint(_if_dap, DIR_IN,  TRANS_BULK);

    // Prepare new request to receive a DAP request
    _ep_dap_out->start_transfer(_buffer_out.data, TUPP_DEFAULT_PAKET_SIZE);

    // Endpoint OUT handler -> A new DAP request arrived!
    _ep_dap_out->data_handler = [&](const uint8_t *buf, uint16_t len) {
        // Check for DAP TransferAbort
        if (!_dap.check_for_transfer_abort(buf)) {
            // Check for FIFO space
            if (!_received_data.available_put()) {
                TUPP_LOG(LOG_ERROR, "Can not store DAP request in FIFO!");
            }
            // Copy the buffer into the FIFO
            _buffer_out.len = len;
            _received_data.put(_buffer_out);
            // Trigger our run() method to process the request
            resume();
        }
        // Trigger a new transfer
        _ep_dap_out->start_transfer(_buffer_out.data, _ep_dap_out->descriptor.wMaxPacketSize);
    };

    // Endpoint IN handler -> Send a DAP response to the USB host
    _ep_dap_in->data_handler = [&](uint8_t *, uint16_t) {
        if (_transmit_data.get(_buffer_in)) {
            _ep_dap_in->start_transfer(_buffer_in.data, _buffer_in.len);
        }
    };
}

void usb_dap_device::run() {
    usb_buf_t request, response;

    while (true) {
        // Prevent this task from being suspended too early.
        // When a new request arrives within 10 ms, the task
        // stays in the READY state, and we do not waste time
        // in other tasks.
        uint64_t until = millis() + 10;
        while (millis() < until) {
            if (_received_data.available_get() ||
                _transmit_data.available_get()) break;
        }
        if (!_received_data.available_get() &&
            !_transmit_data.available_get()) {
            suspend();
            yield();
        }

        // Process a DAP request if available
        if (_received_data.available_get()) {
            _received_data.get(request);

            // Process the CMSIS DAP request
            auto ret = _dap.process_request(request, response);

            // Check if all the request bytes were consumed.
            if (ret.request_consumed != request.len) {
                TUPP_LOG(LOG_WARNING, "DAP request size was %d, only %d bytes consumed!",
                         request.len, ret.request_consumed);
            }

            if (!_transmit_data.put(response)) {
                TUPP_LOG(LOG_ERROR, "Could not store DAP response in FIFO!");
            }
        }
        // Send a DAP response if available
        if (_transmit_data.available_get()) {
            if (!_ep_dap_in->is_active()) {
                // Parameters are discarded. The handler
                // will check the FIFO for data to send
                _ep_dap_in->data_handler(nullptr, 0);
            }
        }
    }
}
