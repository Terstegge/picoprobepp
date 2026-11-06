target_sources(picoprobe++ PRIVATE
)

# Only compile the LED implementation which is used by the selected board.
if (YAHAL_BOARD STREQUAL "rpi-pico" OR
    YAHAL_BOARD STREQUAL "rpi-pico2")
    target_sources(picoprobe++ PRIVATE
            DAP_led_rpi_pico.cpp
    )
elseif (YAHAL_BOARD STREQUAL "rpi-debug-probe")
    target_sources(picoprobe++ PRIVATE
            DAP_led_rpi_debug_probe.cpp
    )
elseif (YAHAL_BOARD STREQUAL "rp2040-launchpad")
    target_sources(picoprobe++ PRIVATE
            DAP_led_rp2040_launchpad.cpp
    )
elseif (YAHAL_BOARD STREQUAL "rp2350-launchpad")
    target_sources(picoprobe++ PRIVATE
            DAP_led_rp2350_launchpad.cpp
    )
elseif (YAHAL_BOARD STREQUAL "rp2xxx-launchpad-probe")
    target_sources(picoprobe++ PRIVATE
            DAP_led_rp2xxx_launchpad_probe.cpp
    )
endif()

target_include_directories(picoprobe++
        PRIVATE .
)
