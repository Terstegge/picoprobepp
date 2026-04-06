cmake_minimum_required(VERSION 3.20)

# Set the target board
# set(YAHAL_BOARD "rpi-pico")
# set(YAHAL_BOARD "rpi-pico2")
# set(YAHAL_BOARD "rp2040-launchpad")
# set(YAHAL_BOARD "rp2350-launchpad")
set(YAHAL_BOARD "rp2xxx-launchpad-probe")

# Locate the YAHAL library folder
include(YAHAL_import.cmake)

project(picoprobe++)

add_executable(picoprobe++)

# Optimize application for speed
target_compile_options(picoprobe++
        PRIVATE -Ofast
)

# Add YAHAL
yahal_add_me(picoprobe++)

# Set YAHAL system clock to 200MHz for RP2040
if (YAHAL_BOARD STREQUAL "rp2040-launchpad")
target_compile_definitions(YAHAL_picoprobe++
        PRIVATE SYS_CLK_FREQ=200
)
endif()

# Optimize YAHAL for speed
target_compile_options(YAHAL_picoprobe++
        PRIVATE -Ofast
)

# Add WS2812 driver if we use a RP2xxx Launchpad
if (YAHAL_BOARD STREQUAL "rp2040-launchpad")
yahal_add_driver(picoprobe++ rp2040/ws2812)
endif()
if (YAHAL_BOARD STREQUAL "rp2350-launchpad")
    yahal_add_driver(picoprobe++ rp2350/ws2812)
endif()

# Add USB and DAP libraries
yahal_add_library(picoprobe++ tinyUSB++)
yahal_add_library(picoprobe++ openDAP++)

yahal_add_custom_targets(picoprobe++)
yahal_add_extra_outputs(picoprobe++)

add_subdirectory(config)
add_subdirectory(src)
