target_sources(picoprobe++ PRIVATE
        usb_dap_device.cpp
)

target_include_directories(picoprobe++
        PRIVATE .
)

# generate PIO header file
generate_pio_header(picoprobe++
        ${CMAKE_CURRENT_LIST_DIR}/pio/dap_control.pio
        OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/pio)
