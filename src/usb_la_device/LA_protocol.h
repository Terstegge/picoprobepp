///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
/**
 * @file LA_protocol.h
 * @brief
 */
#ifndef LA_PROTOCOL_H
#define LA_PROTOCOL_H

#include <array>
#include "task.h"
#include "usb_la_device.h"
#include "capture.h"

class LA_protocol final : public task {
public:
    explicit LA_protocol(usb_la_device & usb);

    [[noreturn]] void run() override;

private:
    usb_la_device &         _usb;
    capture                 _capture;
    capture_config_t        _capture_config;

    size_t                  _total_bytes_sent {0};
    uint8_t                 _trigger_stage {0};

    std::array<char, 20>    _cmd_str  {};
    std::array<char, 20>    _resp_str {};

    void reset();
};

#endif
