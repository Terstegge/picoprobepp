target_sources(debug_probe PRIVATE
        usb_uart_device.cpp
)

target_include_directories(debug_probe
        PRIVATE .
)