// ---------------------------------------------
//           This file is part of
//      _  _   __    _   _    __    __
//     ( \/ ) /__\  ( )_( )  /__\  (  )
//      \  / /(__)\  ) _ (  /(__)\  )(__
//      (__)(__)(__)(_) (_)(__)(__)(____)
//
//     Yet Another HW Abstraction Library
//      Copyright (C) Andreas Terstegge
//      BSD Licensed (see file LICENSE)
//
// ---------------------------------------------
//
// This file hides away all NCU specific header files
//
#ifndef BOARD_INCLUDES_H
#define BOARD_INCLUDES_H

#include "board.h"

#ifdef YAHAL_BOARD_RP2xxx_LAUNCHPAD_PROBE
#define   DAP_led DAP_led_rp2xxx_launchpad_probe
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
#include "config_rp2xxx_launchpad_probe.h"
#include "DAP_led_rp2xxx_launchpad_probe.h"
#endif

#ifdef YAHAL_BOARD_RP2040_LAUNCHPAD
#define   DAP_led DAP_led_rp2040_launchpad
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
#include "config_rp2040_launchpad.h"
#include "DAP_led_rp2040_launchpad.h"
#endif

#ifdef YAHAL_BOARD_RP2350_LAUNCHPAD
#define   DAP_led DAP_led_rp2350_launchpad
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
#include "config_rp2350_launchpad.h"
#include "DAP_led_rp2350_launchpad.h"
#endif

#endif // BOARD_INCLUDES_H
