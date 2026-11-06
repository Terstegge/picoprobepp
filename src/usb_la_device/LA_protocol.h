#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <array>
#include "task.h"
#include "usb_la_device.h"
#include "capture.h"

class LA_protocol final : public task {
public:
    explicit LA_protocol(usb_la_device & usb);

    [[noreturn]] void run() override;

private:

    static size_t COPY_TO_RAM write_rle4_data  (uint8_t * data, size_t len);
    static void   COPY_TO_RAM write_rle4_sample(uint8_t * & write_ptr,
                                                size_t rle_count,
                                                uint8_t last);

    usb_la_device &         _usb;
    capture                 _capture;
    capture_config_t        _capture_config;

    size_t                  _total_bytes_sent {0};

    std::array<char, 20>    _cmd_str  {};
    std::array<char, 20>    _resp_str {};
};

#endif
