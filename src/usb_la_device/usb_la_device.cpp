///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "usb_la_device.h"
#include "usb_log.h"
#include <cstring>

using namespace TUPP;
using enum bInterfaceClass_t;
using enum bInterfaceSubClass_t;
using enum bInterfaceProtocol_t;
using enum ep_attributes_t;
using enum direction_t;

usb_la_device::usb_la_device(usb_device_controller & controller,
                             usb_configuration & configuration)
: task("LOGIC ANALYZER"), _configuration(configuration)
{
    TUPP_LOG(LOG_DEBUG, "usb_la_device() @%x", this);
    // Set up the CDC-ACM descriptor tree
    _assoc.set_bFunctionClass      (IF_CLASS_CDC);
    _assoc.set_bFunctionSubClass   (IF_SUBCLASS_ABSTRACT_CONTROL_MODEL);
    _assoc.set_bFunctionProtocol   (IF_PROTOCOL_NONE);

    _if_ctrl.set_bInterfaceClass   (IF_CLASS_CDC);
    _if_ctrl.set_bInterfaceSubClass(IF_SUBCLASS_ABSTRACT_CONTROL_MODEL);
    _if_ctrl.set_bInterfaceProtocol(IF_PROTOCOL_NONE);
    _if_ctrl.set_InterfaceName     ("CDC-ACM LA Control");

    _if_data.set_bInterfaceClass   (IF_CLASS_CDC_DATA);
    _if_data.set_bInterfaceProtocol(IF_PROTOCOL_NONE);

    _header_fd.set_bcdCDC(0x0120);
    _call_mgmt_fd.set_bmCapabilities({
        .handle_call_management         = 0,
        .call_management_via_data_class = 0
    });
    _call_mgmt_fd.set_bDataInterface(_if_data.descriptor.bInterfaceNumber);

    _acm_fd.set_bmCapabilities({
        .comm_feature_support       = 0,
        .line_coding_support        = 1,
        .send_break_support         = 1,
        .network_connection_support = 0
    });

    _union_fd.set_bControlInterface     (_if_ctrl.descriptor.bInterfaceNumber);
    _union_fd.set_bSubordinateInterface0(_if_data.descriptor.bInterfaceNumber);

    // Create endpoints
    _ep_data_in  = controller.create_endpoint(_if_data, DIR_IN,  TRANS_BULK);
    _ep_data_out = controller.create_endpoint(_if_data, DIR_OUT, TRANS_BULK);
    _ep_ctrl_in  = controller.create_endpoint(_if_ctrl, DIR_IN,  TRANS_INTERRUPT);

    // Trigger a transfer to receive the first data items from the host
    _ep_data_out->start_transfer(_buffer_out, _ep_data_out->descriptor.wMaxPacketSize);

    // OUT-handler: Copy incoming data to the RX FIFO and trigger a new transfer
    _ep_data_out->data_handler = [&](const uint8_t *buf, const uint16_t len) {
        for (int i = 0; i < len; ++i) {
            _rx_fifo.put(buf[i]);
        }
        resume(); // Wake up task
        _ep_data_out->start_transfer(_buffer_out, _ep_data_out->descriptor.wMaxPacketSize);
    };

    // Dummy handler for CDC control requests.
    _if_ctrl.setup_handler = [&](const setup_packet_t *pkt) {
        switch(pkt->bRequest) {
            case bRequest_t::REQ_CDC_SET_LINE_CODING:
                controller.handler = [&](const uint8_t *, uint16_t) {};
                controller._ep0_out->start_transfer(
                    reinterpret_cast<uint8_t *>(&_line_coding),
                    sizeof(_line_coding)
                );
                break;
            case bRequest_t::REQ_CDC_GET_LINE_CODING:
                controller._ep0_in->start_transfer(
                    reinterpret_cast<uint8_t *>(&_line_coding),
                    sizeof(_line_coding)
                );
                break;
            case bRequest_t::REQ_CDC_SET_CONTROL_LINE_STATE:
            case bRequest_t::REQ_CDC_SEND_BREAK:
                controller._ep0_in->send_zlp_data1();
                break;
            default:
                controller._ep0_in->send_stall(true);
                controller._ep0_out->send_stall(true);
        }
    };
}

char usb_la_device::getc() {
    TUPP_LOG(LOG_DEBUG, "usb_la_getc()");
    uint8_t c = 0;
    while (!_rx_fifo.get(c)) {
        yield();
    }
    return static_cast<char>(c);
}

uint16_t usb_la_device::available() {
    TUPP_LOG(LOG_DEBUG, "available()");
    return _rx_fifo.available_get();
}

void usb_la_device::write_response(const char * str) const {
    TUPP_LOG(LOG_DEBUG, "write_response(%s)", str);
    if (!str || *str == 0) return;
    while (_ep_data_in->is_active()) yield();
    _ep_data_in->start_transfer((uint8_t *)str, strlen(str));
}

void usb_la_device::write_response(const char c) const {
    TUPP_LOG(LOG_DEBUG, "write_response(%c)", c);
    static char buf[2];
    buf[0] = c;
    buf[1] = 0;
    write_response(buf);
}

bool usb_la_device::queue_block(uint8_t* data, const size_t len) {
    TUPP_LOG(LOG_DEBUG, "queue_block()", this);
    const la_block_t block = {data, len};
    const bool success = _transmit_queue.put(block);
    if (success) resume();
    return success;
}

[[noreturn]] void usb_la_device::run() {
    la_block_t current_block;
    while (true) {
        // Prevent this task from being suspended too early.
        // When a new request arrives within 100 ms, the task
        // stays in the READY state, and we do not waste time
        // in other tasks.
        const uint64_t until = millis() + 100;
        while (millis() < until) {
            if (_transmit_queue.available_get()) break;
        }

        // Resume the task if there is nothing to do...
        if (!_transmit_queue.available_get()) {
            suspend();
            yield();
        }

        // Check for new bulk transmits to the host
        if (_transmit_queue.available_get()) {
            // Only start transfer if IN endpoint is not active
            if (!_ep_data_in->is_active()) {
                // Get the block from the FIFO and transmit data via USB
                _transmit_queue.get(current_block);
                _ep_data_in->start_transfer(current_block.data, current_block.len);
            }
        }
    }
}
