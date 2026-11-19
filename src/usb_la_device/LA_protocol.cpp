///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
/**
 * @file LA_protocol.cpp
 * @brief Implementation of the sigrok/PulseView Logic Analyzer command protocol.
 */
#include "LA_protocol.h"
#include "usb_log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

LA_protocol::LA_protocol(usb_la_device &usb)
    : task("LA PROTOCOL"), _usb(usb) {
    // Forward prepared RLE or mixed-signal sample blocks from Core 1 to the USB TX queue
    _capture.data_handler = [&](uint8_t *data, const size_t len) {
        _usb.queue_block(data, len);
        _total_bytes_sent += len;
    };
}

[[noreturn]] void LA_protocol::run() {
    while (true) {
        size_t cmd_index{0};
        char c{0};
        bool cmd_received{false};

        // Read a new command line (terminated by \r or \n, except single-char '*' and '+')
        do {
            c = _usb.getc();
            switch (c) {
                case '*': // Immediate reset without line ending
                case '+': // Immediate abort without line ending
                    _cmd_str[cmd_index++] = c;
                    _cmd_str[cmd_index] = 0;
                    cmd_received = true;
                    break;
                case '\r': // Line end -> terminate command string
                case '\n':
                    _cmd_str[cmd_index] = 0;
                    cmd_received = true;
                    break;
                default:
                    if (cmd_index < _cmd_str.size() - 1) {
                        _cmd_str[cmd_index++] = c;
                    } else {
                        TUPP_LOG(LOG_ERROR, "Command buffer overflow!");
                        cmd_index = 0;
                    }
            }
        } while (!cmd_received || cmd_index == 0);

        // Set default acknowledgement response
        strcpy(_resp_str.data(), "*");
        bool send_response = true;

        // Dispatch command in _cmd_str
        switch (_cmd_str[0]) {
            // *: Reset device state
            case '*': {
                TUPP_LOG(LOG_INFO, "Received * -> Reset");
                send_response = false;
                _capture.abort();
                reset();
                break;
            }
            // +: Abort active acquisition
            case '+': {
                TUPP_LOG(LOG_INFO, "Received + -> Abort");
                send_response = false;
                _capture.abort();
                reset();
                break;
            }
            // i: Report device identification and capabilities
            // Reply format: SRPICO,AxxyDzz,vv
            //   xx : Number of analog channels
            //   y  : Bytes per analog sample (2 = 14-bit / 12-bit 2x7-bit encoding)
            //   zz : Number of digital channels
            //   vv : Protocol version (02)
            case 'i': {
                sprintf(_resp_str.data(), "SRPICO,A%02d2D%02d,02",
                        LA_A_CHANNEL_COUNT, LA_D_CHANNEL_COUNT);
                TUPP_LOG(LOG_INFO, "ID: %s", _resp_str.data());
                break;
            }
            // n: Report channel name
            // Format: n<A/D>xx -> Reply: A<gpio> or D<gpio>
            case 'n': {
                char type = _cmd_str[1];
                uint16_t channel = atoi(_cmd_str.data() + 2);
                if (type == 'A') {
                    sprintf(_resp_str.data(), "A%d",
                            channel + LA_A_CHANNEL_START);
                } else if (type == 'D') {
                    sprintf(_resp_str.data(), "D%d",
                            channel + LA_D_CHANNEL_START);
                }
                TUPP_LOG(LOG_INFO, "Name for channel %c%d set to %s",
                         type, channel, _resp_str.data());
                break;
            }
            // a: Report analog scale and offset in microvolts
            // Format: axx -> Reply: <uV per LSB>x<offset in uV>
            case 'a': {
                int channel = atoi(_cmd_str.data() + 1);
                if (channel >= 0) {
                    // 3.3 V / 4096 (12-bit ADC) = 805.66 uV per LSB, 0 V offset
                    sprintf(_resp_str.data(), "806x0");
                    TUPP_LOG(LOG_INFO, "Analog scale for A%d set", channel);
                }
                break;
            }
            // D: Enable or disable a digital channel
            // Format: Dxyy (x: 0=disable, 1=enable; yy: channel index)
            case 'D': {
                uint8_t ch = atoi(_cmd_str.data() + 2);
                if (ch >= LA_D_CHANNEL_COUNT) {
                    send_response = false;
                    break;
                }
                if (_cmd_str[1] == '1') {
                    _capture_config.digital_channels_mask |= (1u << ch);
                } else {
                    _capture_config.digital_channels_mask &= ~(1u << ch);
                }
                TUPP_LOG(LOG_INFO, "D channel mask: 0x%08X",
                         _capture_config.digital_channels_mask);
                break;
            }
            // A: Enable or disable an analog channel
            // Format: Axyy (x: 0=disable, 1=enable; yy: channel index)
            case 'A': {
                uint8_t ch = atoi(_cmd_str.data() + 2);
                if (ch >= LA_A_CHANNEL_COUNT) {
                    send_response = false;
                    break;
                }
                if (_cmd_str[1] == '1') {
                    _capture_config.analog_channels_mask |= (1u << ch);
                } else {
                    _capture_config.analog_channels_mask &= ~(1u << ch);
                }
                TUPP_LOG(LOG_INFO, "A channel mask: 0x%02X",
                         _capture_config.analog_channels_mask);
                break;
            }
            // R: Set sample rate in Hz (Format: R<rate>)
            case 'R': {
                uint32_t rate = atol(_cmd_str.data() + 1);
                if (rate >= 5000 && rate <= 200000000) {
                    _capture_config.sample_rate = rate;
                    TUPP_LOG(LOG_INFO, "Sample rate set to: %u Hz", rate);
                } else {
                    TUPP_LOG(LOG_ERROR, "Unsupported sample rate: %s",
                             _cmd_str.data());
                    send_response = false;
                }
                break;
            }
            // L: Set post-trigger sample limit (Format: L<#samples>)
            case 'L': {
                uint32_t limit = atol(_cmd_str.data() + 1);
                _capture_config.post_trigger_samples = limit;
                TUPP_LOG(LOG_INFO, "Sample limit set to: %u", limit);
                break;
            }
            // p: Set pre-trigger sample count (Format: p<#samples>)
            case 'p': {
                uint32_t count = atol(_cmd_str.data() + 1);
                _capture_config.pre_trigger_samples = count;
                TUPP_LOG(LOG_INFO, "Pre-trigger samples set to: %u", count);
                break;
            }
            // t: Configure a hardware trigger condition
            // Format: tvxx (v: 0=LOW, 1=HIGH, 2=RISING, 3=FALLING, 4=EDGE; xx: channel index)
            case 't': {
                uint8_t channel = atoi(_cmd_str.data() + 2);
                if (channel >= LA_D_CHANNEL_COUNT || _trigger_stage >=
                    LA_D_CHANNEL_COUNT) {
                    TUPP_LOG(LOG_ERROR, "Invalid trigger index %d or stage %d",
                             channel, _trigger_stage);
                    send_response = false;
                    break;
                }
                switch (_cmd_str[1]) {
                    case '0': {
                        TUPP_LOG(LOG_INFO,
                                 "Setting Trigger LOW for channel %d at stage %d",
                                 channel, _trigger_stage);
                        _capture_config.trigger_stages[_trigger_stage].set(
                            LEVEL_LOW, channel + LA_D_CHANNEL_START);
                        break;
                    }
                    case '1': {
                        TUPP_LOG(LOG_INFO,
                                 "Setting Trigger HIGH for channel %d at stage %d",
                                 channel, _trigger_stage);
                        _capture_config.trigger_stages[_trigger_stage].set(
                            LEVEL_HIGH, channel + LA_D_CHANNEL_START);
                        break;
                    }
                    case '2': {
                        if (_capture_config.trigger_stages[_trigger_stage].
                            is_set()) {
                            _trigger_stage++;
                        }
                        if (_trigger_stage < LA_D_CHANNEL_COUNT) {
                            TUPP_LOG(LOG_INFO,
                                     "Setting Trigger RISING for channel %d at stage %d",
                                     channel, _trigger_stage);
                            _capture_config.trigger_stages[_trigger_stage++].
                                    set(
                                        RISING, channel + LA_D_CHANNEL_START);
                        }
                        break;
                    }
                    case '3': {
                        if (_capture_config.trigger_stages[_trigger_stage].
                            is_set()) {
                            _trigger_stage++;
                        }
                        if (_trigger_stage < LA_D_CHANNEL_COUNT) {
                            TUPP_LOG(LOG_INFO,
                                     "Setting Trigger FALLING for channel %d at stage %d",
                                     channel, _trigger_stage);
                            _capture_config.trigger_stages[_trigger_stage++].
                                    set(
                                        FALLING, channel + LA_D_CHANNEL_START);
                        }
                        break;
                    }
                    case '4': {
                        if (_capture_config.trigger_stages[_trigger_stage].
                            is_set()) {
                            _trigger_stage++;
                        }
                        if (_trigger_stage < LA_D_CHANNEL_COUNT) {
                            TUPP_LOG(LOG_INFO,
                                     "Setting Trigger EDGE for channel %d at stage %d",
                                     channel, _trigger_stage);
                            _capture_config.trigger_stages[_trigger_stage++].
                                    set(
                                        EDGE, channel + LA_D_CHANNEL_START);
                        }
                        break;
                    }
                    default: {
                        TUPP_LOG(LOG_ERROR, "Unsupported trigger type %c",
                                 _cmd_str[1]);
                    }
                }
                break;
            }
            // F: Start untriggered capture
            // C: Start triggered capture
            case 'F':
            case 'C': {
                TUPP_LOG(LOG_INFO, "Start sampling with command %c",
                         _cmd_str[0]);
                send_response = false;
                _capture_config.digital_channels_base = LA_D_CHANNEL_START;
                _capture_config.analog_channel_base = LA_A_CHANNEL_START;
                _total_bytes_sent = 0;
                if (_capture.start(&_capture_config)) {
                    while (_capture.is_active()) {
                        sleep_ms(5);
                        if (_usb.available()) {
                            char cmd = _usb.getc();
                            TUPP_LOG(LOG_INFO, "Received %c while sampling",
                                     cmd);
                            if (cmd == '+' || cmd == '*') {
                                break;
                            }
                        }
                    }
                    _capture.abort();
                    // Wait until all queued sample packets have been transmitted over USB
                    while (_usb.is_tx_busy()) {
                        sleep_ms(1);
                    }
                    // Give the host driver a brief window to drain the final USB packet
                    task::sleep_ms(50);
                    if (_capture.has_overflow()) {
                        _usb.write_response("!");
                    } else {
                        char buf[20];
                        sprintf(buf, "$%u+", _total_bytes_sent);
                        TUPP_LOG(LOG_INFO, "Sending final byte count '%s'", buf);
                        _usb.write_response(buf);
                    }
                    TUPP_LOG(LOG_INFO, "End sampling");
                } else {
                    TUPP_LOG(LOG_ERROR, "Capture start() failed!");
                }
                reset();
                break;
            }
            default: {
                TUPP_LOG(LOG_ERROR, "Unsupported command: %s", _cmd_str.data());
                send_response = false;
                break;
            }
        }
        if (send_response) {
            _usb.write_response(_resp_str.data());
        }
    }
}

void LA_protocol::reset() {
    _capture_config.digital_channels_mask = 0;
    _capture_config.analog_channels_mask = 0;
    _total_bytes_sent = 0;
    _trigger_stage = 0;
    for (auto &t: _capture_config.trigger_stages) {
        t.reset();
    }
}
