#include "LA_protocol.h"
#include "usb_log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

LA_protocol::LA_protocol(usb_la_device & usb)
: task("LA PROTOCOL"), _usb(usb) {

    // This handler will be called when the capture class has
    // prepared a block of data, which should be sent to the
    // host (e.g. to pulseview via USB).
    _capture.data_handler = [&](uint8_t* data, const size_t len) {
        _usb.queue_block(data, len);
        _total_bytes_sent += len;
    };
}

[[noreturn]] void LA_protocol::run() {
    while(true) {
        size_t cmd_index {0};
        char c {0};
        bool cmd_received {false};
        // Read in a new command line (up to \r or \n)
        do {
            c = _usb.getc();
            switch(c) {
                case '*':   // These commands work without a \n or \r!
                case '+':
                    _cmd_str[cmd_index++] = c;
                    _cmd_str[cmd_index] = 0;
                    cmd_received = true;
                    break;
                case '\r':  // Line end -> terminate string
                case '\n':
                    _cmd_str[cmd_index] = 0;
                    cmd_received = true;
                    break;
                default:
                    if (cmd_index < _cmd_str.size()-1) {
                        _cmd_str[cmd_index++] = c;
                    } else {
                        TUPP_LOG(LOG_ERROR, "Command Buffer Overflow!");
                        cmd_index = 0;
                    }
            }
        } while (!cmd_received || cmd_index == 0);

        // Set default response string
        strcpy(_resp_str.data(), "*");
        bool send_response = true; // Default: Send a response

        // Process the command in _cmdstr
        switch(_cmd_str[0]) {
            // *: Reset
            case '*': {
                TUPP_LOG(LOG_INFO, "Received * -> Reset?");
                send_response = false;
                _capture.abort();
                reset();
                break;
            }
            // +: Abort
            case '+': {
                TUPP_LOG(LOG_INFO, "Received + -> Abort/End?");
                send_response = false;
                _capture.abort();
                reset();
                break;
            }
            // i: Report Device information
            // Reply: SRPICO,AxxyDzz,vv
            // xx,zz: Number of analog/digital channels
            // y:     Analog sample size (1)
            // vv:    Version (02)
            case 'i': {
                sprintf(_resp_str.data(), "SRPICO,A%02d1D%02d,02",
                        LA_A_CHANNEL_COUNT, LA_D_CHANNEL_COUNT);
                TUPP_LOG(LOG_INFO, "ID: %s", _resp_str.data());
                break;
            }
            // n: Report a channel name
            // Format: n<A/D>xx
            // A/D: Select channel type A=analog D=digital
            // xx:  The channel number
            // Reply: <name>
            case 'n': {
                char type = _cmd_str[1];
                uint16_t channel = atoi(_cmd_str.data()+2);
                if (type == 'A') {
                    sprintf(_resp_str.data(), "A%d",
                        channel + LA_A_CHANNEL_START);
                }
                if (type == 'D') {
                    sprintf(_resp_str.data(), "D%d",
                        channel + LA_D_CHANNEL_START);
                }
                TUPP_LOG(LOG_INFO, "Name for channel %c%d set to %s",
                    type, channel, _resp_str.data());
                break;
            }
            // a: Report the analog scale
            // Format: axx
            // xx: Analog channel number
            // Reply: <resolution in uV>x<voltage offset in uV>
            case 'a': {
                int channel = atoi(_cmd_str.data()+1);
                if (channel >= 0) {
                    // scale and offset are both in integer uVolts separated by x
                    sprintf(_resp_str.data(), "25781x0"); // 3.3/(2^7) and 0V offset
                    TUPP_LOG(LOG_INFO, "Analog Scale for A%d set", channel);
                }
                break;
            }
            // D: enable/disable digital channel
            // Format Dxyy
            // x:  0 disable channel, 1 enable channel
            // yy: Channel number
            case 'D': {
                if (_cmd_str[1] == '0') break;
                uint8_t d_channels = atoi(_cmd_str.data()+2) + 1;
                if (d_channels > _capture_config.number_of_digital_channels) {
                    _capture_config.number_of_digital_channels = d_channels;
                    TUPP_LOG(LOG_INFO, "Number of D channels set to %d", d_channels);
                }
                break;
            }
            // A: enable/disable analog channel
            // Format Axyy
            // x:  0 disable channel, 1 enable channel
            // yy: Channel number
            case 'A': {
                if (_cmd_str[1] == '0') break;
                uint8_t a_channels = atoi(_cmd_str.data()+2) + 1;
                if (a_channels > _capture_config.number_of_analog_channels) {
                    _capture_config.number_of_analog_channels = a_channels;
                    TUPP_LOG(LOG_INFO, "Number of A channels set to %d", a_channels);
                }
                break;
            }
            // R: Sample rate
            // Format R<rate>
            case 'R': {
                uint32_t rate = atol(_cmd_str.data()+1);
                if (rate >= 5000 && rate <= 200000000) {
                    _capture_config.sample_rate = rate;
                    TUPP_LOG(LOG_INFO, "Sample Rate set to: %d", rate);
                } else {
                    TUPP_LOG(LOG_ERROR, "Unsupported sample rate: %s", _cmd_str.data());
                    send_response = false;
                }
                break;
            }
            // L: Sample Limit
            // Format: L<#samples>
            case 'L': {
                uint32_t limit = atol(_cmd_str.data()+1);
                _capture_config.post_trigger_samples = limit;
                TUPP_LOG(LOG_INFO, "Sample Limit set to: %d", limit);
                break;
            }
            // p: Pretrigger sample count
            // Format: p<#samples>
            case 'p': {
                uint32_t count = atol(_cmd_str.data()+1);
                _capture_config.pre_trigger_samples = count;
                TUPP_LOG(LOG_INFO, "Pretrigger samples set to: %d", count);
                break;
            }
            // t: Set a trigger
            // Format: tvxx
            // v:  trigger type 0=LOW, 1=HIGH, 2=RISING, 3=FALLING, 4=EDGE
            // xx: digital channel index (0...n)
            case 't': {
                // Format tvxx where v is value and xx is two digit
                uint8_t channel = atoi(_cmd_str.data()+2);
                if (channel >= LA_D_CHANNEL_COUNT) {
                    TUPP_LOG(LOG_ERROR, "Wrong Trigger Index %d", channel);
                    send_response = false;
                    break;
                }
                switch (_cmd_str[1]) {
                    case '0': {
                        TUPP_LOG(LOG_INFO, "Setting Trigger LOW for channel %d at stage %d",
                            channel, _trigger_stage);
                        _capture_config.trigger_stages[_trigger_stage].
                            set(LEVEL_LOW, channel+LA_D_CHANNEL_START);
                        break;
                    }
                    case '1': {
                        TUPP_LOG(LOG_INFO, "Setting Trigger HIGH for channel %d at stage %d",
                            channel, _trigger_stage);
                        _capture_config.trigger_stages[_trigger_stage].
                            set(LEVEL_HIGH, channel+LA_D_CHANNEL_START);
                        break;
                    }
                    case '2': {
                        if (_capture_config.trigger_stages[_trigger_stage].is_set()) {
                            _trigger_stage++;
                        }
                        TUPP_LOG(LOG_INFO, "Setting Trigger RISING for channel %d at stage %d",
                            channel, _trigger_stage);
                        _capture_config.trigger_stages[_trigger_stage++].
                            set(RISING, channel+LA_D_CHANNEL_START);
                        break;
                    }
                    case '3': {
                        if (_capture_config.trigger_stages[_trigger_stage].is_set()) {
                            _trigger_stage++;
                        }
                        TUPP_LOG(LOG_INFO, "Setting Trigger FALLING for channel %d at stage %d",
                            channel, _trigger_stage);
                        _capture_config.trigger_stages[_trigger_stage++].
                            set(FALLING, channel+LA_D_CHANNEL_START);
                        break;
                    }
                    case '4': {
                        if (_capture_config.trigger_stages[_trigger_stage].is_set()) {
                            _trigger_stage++;
                        }
                        TUPP_LOG(LOG_INFO, "Setting Trigger EDGE for channel %d at stage %d",
                            channel, _trigger_stage);
                        _capture_config.trigger_stages[_trigger_stage++].
                            set(EDGE, channel+LA_D_CHANNEL_START);
                        break;
                    }
                    default: {
                        TUPP_LOG(LOG_ERROR, "Unsupported Trigger Type %c", _cmd_str[1]);
                    }
                }
                break;
            }

            // F: Start sample process without triggers
            //    and a fixed set of samples (set by L command)
            // Format: F
            // C: Start sample process with triggers
            //    and a fixed set of samples (set by L and p commands)
            // Format: C
            case 'F':
            case 'C': {
                TUPP_LOG(LOG_INFO, "Start Sampling with command %c", _cmd_str[0]);
                send_response = false;

                _capture_config.digital_channels_base = LA_D_CHANNEL_START;
                _capture_config.analog_channel_base   = LA_A_CHANNEL_START;
                _total_bytes_sent = 0;

                if (_capture.start(&_capture_config)) {
                    while (_capture.is_active()) {
                        sleep_ms(5);
                        if (_usb.available()) {
                            char cmd = _usb.getc();
                            TUPP_LOG(LOG_INFO, "Received %c while sampling", cmd);
                            if (cmd == '+' || cmd == '*') {
                                break;
                            }
                        }
                    }
                    _capture.abort();

                    while (_usb.is_tx_busy()) {
                        sleep_ms(1);
                    }

                    // Add some time so pulseview can process the last samples
                    task::sleep_ms(50);

                    char buf[20];
                    sprintf(buf, "$%d+", _total_bytes_sent);
                    TUPP_LOG(LOG_INFO, "Sending final byte count '%s'", buf);
                    _usb.write_response(buf);
                    TUPP_LOG(LOG_INFO, "End Sampling...");
                } else {
                    TUPP_LOG(LOG_ERROR, "Capture start() failed!!");
                }
                reset();
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
 void LA_protocol::reset() {
    _capture_config.number_of_digital_channels = 0;
    _capture_config.number_of_analog_channels = 0;
    _total_bytes_sent = 0;
    _trigger_stage = 0;
    for (auto & t : _capture_config.trigger_stages) {
        t.reset();
    }
 }
