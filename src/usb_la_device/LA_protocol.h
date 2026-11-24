///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
/**
 * @file LA_protocol.h
 * @brief Command parser and state machine for the sigrok/PulseView
 *        Logic Analyzer protocol.
 *
 * Implements the text-based serial command protocol used by the
 * libsigrok `srpico` driver over a dedicated USB CDC interface.
 */
#ifndef LA_PROTOCOL_H
#define LA_PROTOCOL_H

#include <array>
#include "task.h"
#include "usb_la_device.h"
#include "capture.h"

/**
 * @brief RTOS task handling sigrok protocol commands and capture lifecycle.
 */
class LA_protocol final : public task {
public:
    /**
     * @brief Construct the Logic Analyzer protocol task.
     * @param usb Reference to the USB CDC device dedicated to sigrok communication.
     */
    explicit LA_protocol(usb_la_device & usb);

    /**
     * @brief Main task loop reading commands from USB and controlling the capture engine.
     */
    [[noreturn]] void run() override;

private:
    usb_la_device &         _usb;
    capture                 _capture;
    capture_config_t        _capture_config;

    size_t                  _total_bytes_sent {0};
    uint8_t                 _trigger_stage {0};

    std::array<char, 64>    _cmd_str  {};
    std::array<char, 64>    _resp_str {};

    /**
     * @brief Reset channel masks, byte counters, and all configured trigger stages.
     */
    void reset();
};

#endif // LA_PROTOCOL_H
