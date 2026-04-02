cmake_minimum_required(VERSION 3.20)

# Set the target board
set(YAHAL_BOARD "rp2xxx-launchpad-probe")
# set(YAHAL_BOARD "rp2040-launchpad")
# set(YAHAL_BOARD "rp2350-launchpad")

# Locate the YAHAL library folder
include(YAHAL_import.cmake)

project(debug_probe)

add_executable(debug_probe)

# Optimize application for speed
target_compile_options(debug_probe
        PRIVATE -Ofast
)

# Add YAHAL
yahal_add_me(debug_probe)

# Set YAHAL system clock to 200MHz
target_compile_definitions(YAHAL_debug_probe
        PRIVATE SYS_CLK_FREQ=200
)

# Optimize YAHAL for speed
target_compile_options(YAHAL_debug_probe
        PRIVATE -Ofast
)

# Add WS2812 driver if we use the RP2040 Lauchpad
if (YAHAL_BOARD STREQUAL "rp2040-launchpad")
yahal_add_driver(debug_probe rp2040/ws2812)
endif()
if (YAHAL_BOARD STREQUAL "rp2350-launchpad")
    yahal_add_driver(debug_probe rp2350/ws2812)
endif()

# Add USB and DAP libraries
yahal_add_library(debug_probe tinyUSB++)
yahal_add_library(debug_probe openDAP++)

yahal_add_custom_targets(debug_probe)
yahal_add_extra_outputs(debug_probe)

add_subdirectory(config)
add_subdirectory(src)
