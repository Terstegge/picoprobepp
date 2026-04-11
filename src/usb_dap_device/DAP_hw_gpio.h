///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// DAP HW implementation for the RP2040 MCU using
// simple bit banging.
//
#ifndef DAP_HW_GPIO_H
#define DAP_HW_GPIO_H

#include "config.h"
#include "task.h"
#include "DAP_hw_interface.h"
#include "DAP_log.h"

class DAP_hw_gpio : public DAP_hw_interface {
public:

    explicit DAP_hw_gpio()
    : _swdio_tms(GPIO_SWDIO_TMS), _swclk_tck(GPIO_SWCLK_TCK),
      _reset(GPIO_RESET), _tdi(GPIO_TDI), _tdo(GPIO_TDO) {
        DAP_LOG(DAP_log::LOG_DEBUG, "DAP_hw_gpio()");
    }

    inline void cycle_swclk_tck() {
        swclk_tck_set(true);
        delay_edge();
        swclk_tck_set(false);
        delay_edge();
    }

    ///////////////////////////////
    // Timing configuration methods
    ///////////////////////////////

    // Set HW frequency in Hz, which drives the SWCLK/TCK Pin.
    inline void frequency_set(uint32_t f) override {
        DAP_LOG(DAP_log::LOG_DEBUG, "frequency_set(%d)", f);
        _frequency = f;
    }

    // Delay for a number of microseconds, which is
    // used at some places in the CMSIS DAP protocol.
    void delay_us(uint32_t us) override {
        task::sleep_us(us);
    }

    inline constexpr bool test_domain_timer_support() override {
        return true;
    }
    inline uint32_t test_domain_timer_frequency() override {
        return 1000000;
    }
    inline uint32_t test_domain_timer_get() override {
        return task::micros();
    }

    ////////////////////////////
    // Pin configuration methods
    ////////////////////////////

    // Set up all needed HW Pins for JTAG operation
    // (TCK, TMS, TDI, TDO and optionally nTRST and nRESET)
    void connect_jtag_pins() override {
        DAP_LOG(DAP_log::LOG_DEBUG, "connect_jtag_pins()");
        _swclk_tck.gpioMode(GPIO::INPUT | GPIO::OUTPUT);
        _swdio_tms.gpioMode(GPIO::INPUT | GPIO::OUTPUT);
        _reset.gpioMode    (GPIO::INPUT | GPIO::OUTPUT | GPIO::INIT_HIGH);
        _tdi.gpioMode      (GPIO::INPUT | GPIO::OUTPUT);
        _tdo.gpioMode      (GPIO::INPUT);
    }

    // Set up all needed HW Pins for SWD operation
    // (SWCLK, SWDIO and optionally nRESET)
    void connect_swd_pins() override {
        DAP_LOG(DAP_log::LOG_DEBUG, "connect_swd_pins()");
        _swclk_tck.gpioMode(GPIO::INPUT | GPIO::OUTPUT);
        _swdio_tms.gpioMode(GPIO::INPUT | GPIO::OUTPUT);
        _reset.gpioMode    (GPIO::INPUT | GPIO::OUTPUT | GPIO::INIT_HIGH);
    }

    // De-configure all SWD/JTAG Pins and put them
    // into a high-Z state
    void disconnect() override {
        DAP_LOG(DAP_log::LOG_DEBUG, "disconnect()");
        _swclk_tck.gpioMode(GPIO::INPUT);
        _swdio_tms.gpioMode(GPIO::INPUT);
        _reset.gpioMode    (GPIO::INPUT);
        _tdi.gpioMode      (GPIO::INPUT);
        _tdo.gpioMode      (GPIO::INPUT);
    }

    ///////////////////////////
    // Common operation methods
    ///////////////////////////

    // Method for toggling the SWCLK/TCK line for a
    // certain amount of cycles. Used by SWD and JTAG.
    // Starting level of the SWCLK/TCK line is LOW,
    // so ony cycle is a -> HIGH -> LOW transition
    // (using the configured frequency).
    inline void clock_cycle(uint16_t cycles) override {
        DAP_LOG(DAP_log::LOG_DEBUG, "swclk_cycle(%d)", cycles);
        while (cycles--) {
            cycle_swclk_tck();
        }
    }

    /////////////////////////
    // SWD read write methods
    /////////////////////////

    inline uint32_t swd_read(uint8_t size) override {
        DAP_LOG(DAP_log::LOG_DEBUG, "swd_read(%d)", size);
        swd_swdio_enable_output(false);
        uint32_t value = 0;
        uint32_t bit;
        for (uint8_t i = 0; i < size; i++) {
            bit = swdio_tms_get();
            value |= (bit << i);
            // Clock cycle
            cycle_swclk_tck();
        }
        return value;
    }

    inline void swd_write(uint32_t value, uint8_t size ) override {
        DAP_LOG(DAP_log::LOG_DEBUG, "swd_write(0x%x, %d)", value, size);
        swd_swdio_enable_output(true);
        for (uint8_t i = 0; i < size; i++) {
            swdio_tms_set(value & 1);
            value >>= 1;
            // Clock cycle
            cycle_swclk_tck();
        }
    }

    void swd_swdio_enable_output(bool b) override {
        if (b) _SIO_::SIO.GPIO_OE_SET = (1 << GPIO_SWDIO_TMS);
        else   _SIO_::SIO.GPIO_OE_CLR = (1 << GPIO_SWDIO_TMS);
    }

    //////////////////////////
    // JTAG read write methods
    //////////////////////////

    inline uint32_t jtag_read(uint8_t size) override {
        DAP_LOG(DAP_log::LOG_DEBUG, "jtag_read(%d)", size);
        uint32_t value = 0;
        uint32_t bit;
        for (uint8_t i = 0; i < size; i++) {
            bit = tdo_get();
            value |= (bit << i);
            // Clock cycle
            cycle_swclk_tck();
        }
        return value;
    }

    inline uint32_t jtag_write(uint32_t value, uint8_t size) override {
        DAP_LOG(DAP_log::LOG_DEBUG, "jtag_write(0x%x, %d)", value, size);
        for (uint8_t i = 0; i < size; i++) {
            tdi_set(value & 1);
            value >>= 1;
            // Clock cycle
            cycle_swclk_tck();
        }
        return value;
    }

    inline uint32_t jtag_read_write(uint32_t value, uint8_t size) override {
        DAP_LOG(DAP_log::LOG_DEBUG, "jtag_read_write(0x%x, %d)", value, size);
        uint32_t read_value = 0;
        uint32_t bit;
        for (uint8_t i = 0; i < size; i++) {
            tdi_set(value & 1);
            value >>= 1;
            bit = tdo_get();
            read_value |= (bit << i);
            // Clock cycle
            cycle_swclk_tck();
        }
        return read_value;
    }

    /////////////////////////////
    // Direct SWD/JTAG Pin access
    /////////////////////////////

    // When using bit-banging, this method will wait
    // the correct amount of time to achieve the needed
    // frequency of the SWCLK/TCK signal. Because this
    // method is called after every SWCLK/TCK edge, half
    // of the period duration of the currently selected
    // frequency is needed. So when e.g. the frequency is
    // set to 1MHz, this method would wait approx. 500ns.

    inline void delay_edge() {
        // We don't delay after a SWCLK edge
        // -> maximum speed!
        //delay_us(1);
    }

    inline void swdio_tms_set(bool v) override {
        if (v) _SIO_::SIO.GPIO_OUT_SET = (1 << GPIO_SWDIO_TMS);
        else   _SIO_::SIO.GPIO_OUT_CLR = (1 << GPIO_SWDIO_TMS);
    }

    inline bool swdio_tms_get() override {
        return _SIO_::SIO.GPIO_IN & (1 << GPIO_SWDIO_TMS);
    }

    inline void swclk_tck_set(bool v) override {
        if (v) _SIO_::SIO.GPIO_OUT_SET = (1 << GPIO_SWCLK_TCK);
        else   _SIO_::SIO.GPIO_OUT_CLR = (1 << GPIO_SWCLK_TCK);
    }

    inline bool swclk_tck_get() override {
        return _SIO_::SIO.GPIO_IN & (1 << GPIO_SWCLK_TCK);
    }

    inline void tdi_set(bool v) override {
        if (v) _SIO_::SIO.GPIO_OUT_SET = (1 << 16);
        else   _SIO_::SIO.GPIO_OUT_CLR = (1 << 16);
    }

    inline bool tdi_get() override {
        return _SIO_::SIO.GPIO_IN & (1 << 16);
    }

    inline bool tdo_get() override {
        return _SIO_::SIO.GPIO_IN & (1 << 17);
    }

    inline void trst_set(bool) override {
    }

    inline bool trst_get() override {
        return false;
    }

    inline bool reset_set(bool) override {
        return false;
    }

    inline bool reset_get() override {
        return false;
    }

private:
    gpio_rp2xxx _swdio_tms;
    gpio_rp2xxx _swclk_tck;

    gpio_rp2xxx _reset;
    gpio_rp2xxx _tdi;
    gpio_rp2xxx _tdo;

    uint32_t    _frequency {0};
};

#endif // DAP_HW_GPIO_H
