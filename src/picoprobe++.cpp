///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "config.h"

#include "usb_bos.h"
#include "usb_dcd.h"
#include "usb_device.h"
#include "usb_device_controller.h"

#include "usb_ms_OS_20_capability.h"
#include "usb_ms_func_subset.h"
#include "usb_ms_compatible_ID.h"
#include "usb_ms_registry_property.h"
using enum TUPP::wPropertyDataType_t;

#include "usb_dap_device.h"
#include "usb_uart_device.h"
#include "DAP_hw_rp2040_pio.h"
#include "DAP_log.h"

#include "posix_io.h"
#include "task.h"
#include "task_monitor.h"

int main() {
    // Get the unique ID of this board
    auto id = unique_id_rp2xxx::read_unique_id_string();

    #ifdef DEBUG_UART_ENABLE
    // Debug UART (same as back channel UART)
    uart_rp2xxx uart(DEBUG_UART_TX_GPIO, DEBUG_UART_RX_GPIO);
    posix_io::inst.register_stdio(uart);
    #endif

    // Logging control
    usb_log::inst.setLevel(usb_log::LOG_INFO);
    DAP_log::inst.setLevel(DAP_log::log_level::LOG_INFO);

    // Set up USB Device driver, USB device and
    // generic device controller on top
    usb_dcd & driver = usb_dcd::inst();
    usb_device device;
    usb_device_controller controller(driver, device);

    // USB device descriptor
    device.set_bcdUSB          (USB_DEV_bcdUSB);
    device.set_bMaxPacketSize0 (USB_DEV_bMaxPacketSize0);
    device.set_idVendor        (USB_DEV_VID);
    device.set_idProduct       (USB_DEV_PID);
    device.set_Manufacturer    (USB_DEV_Manufacturer);
    device.set_Product         (USB_DEV_Product);
    device.set_SerialNumber    (id.data());
    device.set_bcdDevice       (USB_DEV_bcdDevice);

    // USB configuration descriptor
    usb_configuration config(device);
    config.set_bConfigurationValue( 1 );
    config.set_bmAttributes({ .remote_wakeup = 0,
                              .self_powered  = 0,
                              .bus_powered   = 1});
    config.set_bMaxPower_mA(100);

    // Set up CMSIS DAP device
    DAP_hw_rp2040_pio dap_hw; // use PIO implementation
    DAP_Protocol dap(dap_hw);
    dap.set_serial(id.data());
    usb_dap_device dap_device(controller, config, dap);
    dap_device.sign_up();

    // Set up CDC ACM device to target
    uart_rp2xxx bc_uart(UART_TARGET_TX_GPIO, UART_TARGET_RX_GPIO);
    usb_uart_device bc_uart_device(controller, config, bc_uart);
    bc_uart_device.sign_up();
    bc_uart_device.setPriority(90);

    #ifdef DEBUG_USB_UART_ENABLE
    // Set up another CDC ACM device for debugging
    // (connected to stdio of the debugger SW)
    #include "usb_cdc_acm_adapter.h"
    usb_cdc_acm_adapter ua(controller, config);
    posix_io::inst.register_stdio(ua);
    #endif

    // Add BOS and MS OS 2.0 descriptor
    usb_bos bos(controller, device); // Add a Binary Object Store
    usb_ms_OS_20_capability ms_os20(bos);

    // This functional subset is only valid for a
    // specific interface number, here the DAP interface
    usb_ms_func_subset ms_func_subset(ms_os20.header);
    ms_func_subset.set_bFirstInterface(dap_device.get_interface_number());

    // The DAP interface will use the WINUSB driver in Windows
    usb_ms_compatible_ID compat_id(ms_func_subset);
    compat_id.set_compatible_id( "WINUSB" );

    // This GUID specifies CMSIS-DAP v2 interfaces
    usb_ms_registry_property reg_prop(ms_func_subset);
    reg_prop.set_wPropertyDataType(REG_MULTI_SZ);
    reg_prop.add_string( "DeviceInterfaceGUIDs" );
    reg_prop.add_string( "{CDB3B5AD-293B-4663-AA36-1AAE46463776}" );
    reg_prop.add_end_marker();

    // LED handler
    DAP_led leds;
    usb_uart_device::uart_tx_cb = [&]()       { leds.trigger_uart_tx_led(); };
    usb_uart_device::uart_rx_cb = [&]()       { leds.trigger_uart_rx_led(); };
    DAP_Protocol::connected_cb  = [&](bool v) { leds.set_connected_led(v);  };
    DAP_Protocol::running_cb    = [&](bool v) { leds.set_running_led(v);    };

    #ifdef START_TASK_MONITOR
    // Optional: Task Monitor
    task_monitor monitor;
    monitor.sign_up();
    monitor.setPriority(20);
    #endif

    // Activate the USB device
    driver.pullup_enable(true);

    // Wait until USB enumeration has finished
    TUPP_LOG(LOG_INFO, "Waiting for USB connection ...");
    while (!controller.active_configuration) {
        task::sleep_ms(20);
    }

    // Set DCD and DTR after USB has connected
    bc_uart_device.set_dcd_dtr(true, true);

    // Welcome the user by blinking some LEDs :)
    // This also indicates that the USB enumeration
    // worked and the probe is ready to be used!
    leds.welcome();

    TUPP_LOG(LOG_INFO, "Starting the scheduler ...");
    // Get the ball rolling by starting the multitasking
    task::start_scheduler();
}
