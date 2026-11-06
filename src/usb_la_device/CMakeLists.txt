target_sources(picoprobe++ PRIVATE
        usb_la_device.cpp
        capture.cpp
        LA_protocol.cpp
        trigger.cpp
)

target_include_directories(picoprobe++
        PRIVATE .
)

# generate PIO header file
generate_pio_header(picoprobe++
        ${CMAKE_CURRENT_LIST_DIR}/pio/capture.pio
        OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/pio)
