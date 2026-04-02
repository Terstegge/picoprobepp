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
// Abstract interface for handling LEDs on a
// CMSIS DAP compliant debug probe. There are
// two LEDs (connected/running) for the CMSIS
// DAP protocol, and two additional LEDs for
// signalling UART (RX/TX) activity. The drivers
// implementing this interface have to make sure
// that the UART LEDs are turned off automatically
// after a specific follow-up time.
//
#ifndef DAP_LED_INTERFACE_H
#define DAP_LED_INTERFACE_H

class DAP_led_interface {
    // Switch on the LED signalling UART TX
    // activity from the target. After the
    // follow-up time, the LED is switched
    // off automatically.
    virtual void trigger_uart_tx_led() = 0;

    // Switch on the LED signalling UART RX
    // activity to the target. After the
    // follow-up time, the LED is switched
    // off automatically.
    virtual void trigger_uart_rx_led() = 0;

    // Switch on/off the LED signalling the
    // connection status between debugger and target.
    virtual void set_connected_led(bool val) = 0;

    // Switch on/off the LED signalling the
    // 'target running' status
    virtual void set_running_led(bool val) = 0;

    // Show a 'welcome' LED pattern
    virtual void welcome() = 0;
};

#endif // DAP_LED_INTERFACE_H
