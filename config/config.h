///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
// This file hides away all MCU specific header files
//
#ifndef BOARD_INCLUDES_H
#define BOARD_INCLUDES_H

#include "board.h"

#ifdef RP2040
#include "gpio_rp2040.h"
#define   gpio_rp2xxx gpio_rp2040
#include "unique_id_rp2040.h"
#define   unique_id_rp2xxx unique_id_rp2040
#include "uart_rp2040.h"
#define   uart_rp2xxx uart_rp2040
#include "pio_rp2040.h"
#define   pio_rp2xxx pio_rp2040
#include "system_rp2040.h"
#define   system_rp2xxx system_rp2040
#include "timer_rp2040.h"
#define   timer_rp2xxx timer_rp2040
#include "RP2040.h"
#endif

#ifdef RP2350
#include "gpio_rp2350.h"
#define   gpio_rp2xxx gpio_rp2350
#include "unique_id_rp2350.h"
#define   unique_id_rp2xxx unique_id_rp2350
#include "uart_rp2350.h"
#define   uart_rp2xxx uart_rp2350
#include "pio_rp2350.h"
#define   pio_rp2xxx pio_rp2350
#include "system_rp2350.h"
#define   system_rp2xxx system_rp2350
#include "timer_rp2350.h"
#define   timer_rp2xxx timer_rp2350
#include "RP2350.h"
#endif

#ifdef YAHAL_BOARD_RPI_PICO
#include "config_rpi_pico.h"
#include "DAP_led_rpi_pico.h"
#define DAP_led DAP_led_rpi_pico
#endif

#ifdef YAHAL_BOARD_RPI_PICO2
#include "config_rpi_pico2.h"
#include "DAP_led_rpi_pico.h"
#define DAP_led DAP_led_rpi_pico
#endif

#ifdef YAHAL_BOARD_RP2040_LAUNCHPAD
#include "config_rp2040_launchpad.h"
#include "DAP_led_rp2040_launchpad.h"
#define DAP_led DAP_led_rp2040_launchpad
#endif

#ifdef YAHAL_BOARD_RP2350_LAUNCHPAD
#include "config_rp2350_launchpad.h"
#include "DAP_led_rp2350_launchpad.h"
#define DAP_led DAP_led_rp2350_launchpad
#endif

#ifdef YAHAL_BOARD_RP2xxx_LAUNCHPAD_PROBE
#include "config_rp2xxx_launchpad_probe.h"
#include "DAP_led_rp2xxx_launchpad_probe.h"
#define DAP_led DAP_led_rp2xxx_launchpad_probe
#endif

#endif // BOARD_INCLUDES_H
