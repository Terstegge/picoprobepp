add_subdirectory(led_interface)
add_subdirectory(usb_dap_device)
add_subdirectory(usb_uart_device)

target_sources(debug_probe PRIVATE
    debug_probe.cpp
)
