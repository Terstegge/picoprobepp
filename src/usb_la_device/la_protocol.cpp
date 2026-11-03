#include "la_protocol.h"
#include "usb_log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

la_protocol::la_protocol(usb_la_device & usb)
: task("LA PROTOCOL"), _usb(usb) {
    _capture.data_handler = [&](uint8_t* data, size_t len) -> void {
        // for (size_t i = 0; i < len; i++) {
        //     if (i%10) {
        //         data[i] = 0x0c;
        //     } else {
        //         data[i] = 0x03;
        //     }
        // }
        size_t rle_len = write_rle4_data(data, len);
        _usb.queue_block(data, rle_len);
    };
}

[[noreturn]] void la_protocol::run() {
    while(true) {
        size_t cmd_index {0};
        char c {0};
        // Read in a new command line (up to \r or \n)
        while(true) {
            c = _usb.getc();
            // Reset works without CR/LF
            if (c == '*') {
                TUPP_LOG(LOG_INFO, "Reset");
                continue;
            }
            // Abort
            if (c == '+') {
                TUPP_LOG(LOG_INFO, "Abort/End");
                continue;
            }
            // Check for end of line
            if (c == '\r' || c == '\n') {
                _cmd_str[cmd_index] = 0;
                break;
            }
            if (cmd_index < _cmd_str.size()) {
                _cmd_str[cmd_index++] = c;
            } else {
                TUPP_LOG(LOG_ERROR, "Command Buffer Overflow!");
            }
        }

        // Set default response string
        strcpy(_resp_str.data(), "*");
        bool send_response = true;

        // Process the command in _cmdstr
        switch(_cmd_str[0]) {
            // ENTER BOOTSEL
            case 'b': {
                if (strcmp(_cmd_str.data(), "bootsel") != 0) {
                    TUPP_LOG(LOG_ERROR, "Invalid Bootsel Command");
                } else {
                    TUPP_LOG(LOG_INFO, "Enter Bootsel Mode");
                }
                send_response = false;
                break;
            }
            // ID / INFO
            case 'i': {
                sprintf(_resp_str.data(), "SRPICO,A%02d1D%02d,02",
                    LA_A_CHANNEL_COUNT, LA_D_CHANNEL_COUNT);
                TUPP_LOG(LOG_INFO, "ID: %s", _resp_str.data());
                break;
            }
            // RATE
            case 'R': {
                uint32_t rate = atol(_cmd_str.data()+1);
                if ((rate >= 5000) &&
                    (rate <= 120000016)) {
                    _capture_config.sample_rate = rate;
                    TUPP_LOG(LOG_INFO, "Sample Rate set to: %d", rate);
                } else {
                    TUPP_LOG(LOG_ERROR, "Unsupported sample rate: %s", _cmd_str.data());
                    send_response = false;
                }
                break;
            }
            // SAMPLE LIMIT
            case 'L': {
                uint32_t limit = atol(_cmd_str.data()+1);
                if (limit > 0) {
                    _capture_config.post_trigger_samples = limit;
                    TUPP_LOG(LOG_INFO, "Number of Samples set to: %d", limit);
                } else {
                    TUPP_LOG(LOG_ERROR, "Bad Sample Limit: %s", _cmd_str.data());
                    send_response = false;
                }
                break;
            }
            // GET ANALOG SCALE
            case 'a': {
                int channel = atoi(_cmd_str.data()+1);
                if (channel >= 0) {
                    // scale and offset are both in integer uVolts separated by x
                    sprintf(_resp_str.data(), "25700x0"); // 3.3/(2^7) and 0V offset
                    TUPP_LOG(LOG_INFO, "Analog Scale for channel %d set", channel);
                } else {
                    TUPP_LOG(LOG_ERROR, "Bad channel for analog scale: %s", _cmd_str.data());
                }
                break;
            }
            // FIXED SET OF SAMPLES
            case 'F': {
                TUPP_LOG(LOG_INFO, "Fixed Set of Samples");
                send_response = false;

                _capture_config.digital_channels_base       = LA_D_CHANNEL_START;
                _capture_config.number_of_digital_channels  = 4;
                _capture_config.analog_channel_base         = LA_A_CHANNEL_START;
                _capture_config.number_of_analog_channels   = 0;

                bool res = _capture.start(&_capture_config);
                if (!res) {
                    TUPP_LOG(LOG_ERROR, "Capture start() returned false!!");
                }

                while (_capture.is_active()) task::sleep_ms(5);

                if (_usb.available()) {
                    TUPP_LOG(LOG_INFO, "Received %c while sending", _usb.getc());
                }

                // Add some time so pulseview can process the last samples
                task::sleep_ms(50);

                char buf[20];
                sprintf(buf, "$%ld+", _capture_config.post_trigger_samples);
                _usb.write_response(buf);

                TUPP_LOG(LOG_INFO, "End Sampling...");
                break;

                // _uart.putc('$');
                // _uart.putc('5');
                // _uart.putc('0');
                // _uart.putc('0');
                // _uart.putc('0');
                // _uart.putc('+');

                //_uart.putc('\r');
                //_uart.putc('\n');
                //task::sleep_ms(50);
                //_uart.putc('*');

                //                for(size_t i=0; i < _config.post_trigger_samples; i++) {
                // Alle 50 Samples schalten wir Kanal 1 (Bit 3) und Kanal 3 (Bit 5) ein
                //                    if ((i / 50) % 2 == 0) {
                //                        // Bit 5 (0x20) und Bit 3 (0x08) auf 1 setzen
                //                        d = 0x8f;
                //                    } else {
                //                        // Alle Kanäle auf Low
                //                        d = 0x80;
                //                    }
                //                    d = 0x80 | (i & 0xf); //i & 0x78;
                //d |= 4;
                //                    _uart.putc(d);
                //task::sleep_ms(1);
                //                }
                //LA_LOG(LOG_INFO, "Waiting for +...");
                //while(!_uart.available()) ;
                //d = _uart.getc();
                //LA_LOG(LOG_INFO, "Received %c");

            }
            case '+': {
                TUPP_LOG(LOG_INFO, "Received +");
                send_response = false;
                break;
            }
            // CONTINUOUS MODE
            case 'C': {
                TUPP_LOG(LOG_INFO, "Continuous Mode");
                send_response = false;

                _capture_config.digital_channels_base       = LA_D_CHANNEL_START;
                _capture_config.number_of_digital_channels  = 4;
                _capture_config.analog_channel_base         = LA_A_CHANNEL_START;
                _capture_config.number_of_analog_channels   = 0;

                bool res = _capture.start(&_capture_config);
                if (!res) {
                    TUPP_LOG(LOG_ERROR, "Capture start() returned false!!");
                }

                while (_capture.is_active()) task::sleep_ms(5);

                if (_usb.available()) {
                    TUPP_LOG(LOG_INFO, "Received %c while sending", _usb.getc());
                }

                // Add some time so pulseview can process the last samples
                task::sleep_ms(50);

                char buf[20];
                sprintf(buf, "$%ld+", _capture_config.post_trigger_samples);
                _usb.write_response(buf);

                TUPP_LOG(LOG_INFO, "End Sampling...");
                break;
            }
            // TRIGGER
            case 't': {
                // Format tvxx where v is value and xx is two digit
                uint8_t channel = atol(_cmd_str.data()+2);
                if (channel >= LA_D_CHANNEL_START) {
                    TUPP_LOG(LOG_ERROR, "Wrong Trigger Index %d", channel);
                    break;
                }
                uint8_t stage = 1;
                TUPP_LOG(LOG_INFO, "Setting Trigger %c on Channel %d", _cmd_str[1], channel);
                switch (_cmd_str[1]) {
                    case '0': _capture_config.trigger_stages[0].set(LEVEL_LOW,
                                channel+LA_D_CHANNEL_START);
                        break;
                    case '1': _capture_config.trigger_stages[0].set(LEVEL_HIGH,
                                channel+LA_D_CHANNEL_START);
                        break;
                    case '2': _capture_config.trigger_stages[stage++].set(RISING,
                                channel+LA_D_CHANNEL_START);
                        break;
                    case '3': _capture_config.trigger_stages[stage++].set(FALLING,
                                channel+LA_D_CHANNEL_START);
                        break;
                    case '4': _capture_config.trigger_stages[stage++].set(EDGE,
                                channel+LA_D_CHANNEL_START);
                        break;
                    default: {
                        TUPP_LOG(LOG_ERROR, "Unsupported Trigger Type %c", _cmd_str[1]);
                    }
                }
                break;
            }
            // PRETRIGGER COUNT
            case 'p': {
                uint16_t count = atoi(_cmd_str.data()+1);
                TUPP_LOG(LOG_INFO, "Pretrigger count set to: %d", count);
                _capture_config.pre_trigger_samples = count;
                break;
            }
            // ENABLE/DISABLE ANALOG CHANNEL
            case 'A': {
                // Enable/disable Analog channel
                //  format is Axyy where x is 0 for disabled, 1 for enabled and yy is
                //  channel #
                TUPP_LOG(LOG_INFO, "Set Analog Channel: %s", _cmd_str.data());
                break;
            }
            // ENABLE/DISABLE DIGITAL CHANNEL
            case 'D': {
                // Enable/disable digital channel.
                //  format is Dxyy where x is 0 for disabled, 1 for enabled and yy is
                //  channel #
                // Note that this is a fixed number 0..N regardless of channel naming and/or
                // pins enabled
                TUPP_LOG(LOG_INFO, "Set Digital Channel: %s", _cmd_str.data());
                break;
            }
            // SIGNAL NAME
            case 'n': {
                // Get signal name. The name can only be pulled from the device, there is
                // no set because the device has no use for the names. Between the host and
                // device the names are always 0..N for all types, but this mode allows the
                // PIN/board/device names to be communicated. format is n(A/D)xx where A/D
                // selects analog digital, and x is one or two characters representing an
                // integer Note: this functional call may return names for channels that
                // don't exist.  It's up to the caller to only ask for names for channels
                // reported by the ID response
                char type = _cmd_str[1];
                uint16_t channel = atoi(_cmd_str.data()+2);
                TUPP_LOG(LOG_INFO, "Getting Name for channel %c%d", type, channel);
                if (type == 'A') {
                    sprintf(_resp_str.data(), "A%d", channel + LA_A_CHANNEL_START);
                }
                if (type == 'D') {
                    sprintf(_resp_str.data(), "D%d", channel + LA_D_CHANNEL_START);
                }
                break;
            }
            default: {
                TUPP_LOG(LOG_ERROR, "Unsupported Command: %s", _cmd_str.data());
                send_response = false;
                break;
            }
        }
        if (send_response) {
            _usb.write_response(_resp_str.data());
        }
    }
}

size_t COPY_TO_RAM la_protocol::write_rle4_data(uint8_t * data, size_t len) {
    if (data == nullptr || len == 0) return 0;
    uint8_t * write_ptr = data;
    //TUPP_LOG(LOG_INFO, "Data Handler Buf %x Size %d", data, len);
    // Write the first sample via USB
    uint8_t last = data[0];
    *write_ptr++ = (0x80 | last);
    size_t rle_count = 0;
    for (size_t i=1; i<len; i++) {
        if (data[i] == last) {
            rle_count++;
        } else {
            last = data[i];
            write_rle4_sample(write_ptr, rle_count, last);
            rle_count = 0;
        }
    }
    if (rle_count) {
        rle_count--;
        write_rle4_sample(write_ptr, rle_count, last);
    }
    //TUPP_LOG(LOG_INFO, "Leaving Data Handler");
    return write_ptr - data;
}

void COPY_TO_RAM la_protocol::write_rle4_sample(uint8_t * & write_ptr, size_t rle_count, uint8_t next) {
    size_t div=0, mod=0;
    // Check if the last sample was repeated at all
    if (rle_count) {
        while (rle_count >= 640) {
            *write_ptr++ = 0x7f;
            rle_count -= 640;
        }
        div = rle_count >> 3;
        mod = rle_count &  7;
        if (div) {
            *write_ptr++ = div + 47;
        }
        *write_ptr++ = (0x80 | (mod << 4) | next);
    } else {
        *write_ptr++ = (0x80 | next);
    }
}