target_sources(debug_probe PRIVATE
        usb_dap_device.cpp
)

target_include_directories(debug_probe
        PRIVATE .
)

# generate PIO header file
generate_pio_header(debug_probe
        ${CMAKE_CURRENT_LIST_DIR}/pio/dap_control.pio
        OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/pio)
