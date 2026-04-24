# picoprobe++

### A debugger firmware for RP2040 and RP2350 based CMSIS-DAP v2 debug probes.

picoprobe++ is a CMSIS DAP v2.0 compatible debugger firmware written in C++.
It has most of the features and at least the performance of the original
Raspberry Pi [debugprobe](https://github.com/raspberrypi/debugprobe) software,
so it can be used as a replacement if needed. picoprobe++ is not using the
Raspberry Pi pico SDK, tinyusb, FreeRTOS or the ARM CMSIS DAP implementation.
Instead, it is based on the following components:

* [YAHAL RTOS](https://git.fh-aachen.de/yahal/YAHAL),
  an embedded operating system written in C++, supporting ARM and ESP MCUs
* [tinyUSB++](https://github.com/Terstegge/tinyUSBpp), a well-structured
  USB library written in C++
* [openDAP++](https://github.com/Terstegge/openDAPpp), a portable CMSIS-DAP
  implementation written in C++
  
picoprobe++ benefits from the features of the libraries it uses:

* picoprobe++ uses the YAHAL preemptive multitasking, which simplifies the overall
  software structure. There are separate tasks for the CDC ACM UART device and the
  CMSIS-DAP interface. Implementation of tasks is using a Java-style syntax - overriding
  a virtual run() method, which is very easy to read and understand.
  Optionally, the YAHAL task monitor can be enabled in the picoprobe++ configuration file,
  which shows all important task statistics in the debug console every 5 seconds.
* All needed USB descriptors in picoprobe++ are set up during runtime, which is
  a nice feature of tinyUSB++. So e.g. adding a second CDC ACM device for firmware
  debugging purposes is only 2 lines of code. tinyUSB++ also has full support for
  Microsofts OS 2.0 USB descriptor tree. This makes configuration changes in the
  USB descriptors very easy.
* picoprobe++ supports SWD and JTAG, which is a feature of openDAP++. However, SWD
  is by far the most commonly used debug protocol today, so JTAG is of less importance.
* picoprobe++ provides different low level DAP drivers. There is one driver exclusively
  using simple GPIO input/output, and a second driver using the PIO module of the RP2040/RP2350
  (which yields better performace, of course).
  This could be important in cases where the PIO modules are already used for someting else.
* picoprobe++ and the libraries it uses try to use abstract C++ interfaces at various places.
  This results in a better software structure, clearly separating the generic code from the
  code which varies depending on the specific MCU or board. This also facilitates porting
  this software to new MCUs or platforms.

---
### Configuration

Configuration of picoprobe++ starts in `CMakeLists.txt`. At the beginning of
this file you can comment out one of the currently supported board types
(pico, pico2, rpi debug probe, rp2040-launchpad, rp2350-launchpad, rp2xxx-launchpad-probe).
Without changing `CMakeLists.txt`, the configuration default is the RPi Debug Probe board.
For every board type, there is a matching configuration file in folder
`config`. Also the LED driver is depending on the board type - see
`config/config.h` for details. In the board-specific config files, there
are configuration sections for the following items:

* **Firmware debugging** - There are two options to see the debug output of
  picoprobe++: Either with a UART interface (default). Make sure that the
  UART for debugging does not interfere with the UART used for communication
  with the target. The other option is to create a second CDC ACM device,
  which will show up on the host PC. In this case, you will not be able to
  see the debug output before creation of the CDC ACM interface. The USB
  and DAP log levels can be set individually. Optionally, you can start
  the YAHAL task monitor to see the task statistics.

* **USB configuration** - Typical USB device values like VID/PID and others.

* **DAP configuration** - Various settings for the DAP interface, including
  several string constants and booleans defining the support of DAP
  features.

* **GPIO configuration** - The Pins to be used for the JTAG/SWD signals as
  well as the Pins to be used for the UART to communicate with the target.

The specific LED driver classes contain the HW definitions / GPIO pins to
be used for the LEDs.

---
### Openocd and the running LED
Openocd does not really care about the DAP 'running' state, meaning it does
not emit commands to switch on/off the corresponding LED on a debug probe.
The folder `openocd` contains two Openocd board configuration files for the
RP2040 and the RP2350, which 'manually' control the running state based on
Openocd events. Try out these files if the 'running' LED should work on your
debug probe. It might be that the 'running' state is accidentally set after
the first connect to the target board. The picoprobe++ LED handler classes
take this into account, and suppress the first setting of the 'running'
state!

---
### Installation
To compile and install this Software, you need
* an installed [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads),
  (the arm-none-eabi variant)
* git and cmake
* a RP2040 or RP2350 based board, e.g. any kind of
 [Raspberry Pi Pico](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html) or a
 [Raspberry Pi Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html),
 or a board with an embedded debugger like the
 [RP2350 Launchpad](https://github.com/Terstegge/rp2350b-launchpad-PCB)
  
Most of the people being interested in this project will probaly have all that already.
Make sure you clone this repository recursively with all submodules (YAHAL is a submodule, which
itself contains submodules like openDAP++ or tinyUSB++:

`git clone https://github.com/Terstegge/picoprobepp.git --recursive`

You might want to rename the cloned folder from `picoprobepp` to its propper name `picoprobe++`
(special characters like `+` are not allowed in github project names).
Then proceed with the typical cmake build steps:

`mkdir BUILD;   cd BUILD;   cmake ..;   make -j 5`

The BUILD folder will contain the `picoprobe++.uf2` file, which is the file to install on
your target board using the 'mass storage device' mode of the RP2040/RP2350. After flashing
and connecting the board to USB, the debug probe will welcome the user by blinking the
indicator LEDs once. This is only happening after USB enumeration, so the user knows the
debugger is up and running!

---
### TODO
picoprobe++ could be extended with the following features.
Most items in this list do not have a very high priority though.

* Add HID functionality to tinyUSB++ so CMSIS DAP v1.0 could be implemented
* Add full JTAG support (working already but not very well tested)
* Add a 'mass storage device' download mode additionally to the CMSIS DAP interface
* Add Segger Real-Time-Transfer (RTT) functionality
* Add CMSIS DAP SWO and UART functionality
* Extend the additional CDC ACM device (showing the firmware logs)
  with a control menu (switching logging levels etc.)
* Adding a simple logic analyzer using the second M0+ or M33/RISC-V core
  (probably a separate project)

---
### History

picoprobe++ was developed because of UART issues with the original Raspberry Pi picoprobe software.
The [RP2350 Launchpad](https://github.com/Terstegge/rp2350b-launchpad-PCB) has some add-on boards
like the [WifiTick board](https://github.com/Terstegge/WiFiTick). The ESP8266 on this board is
programmed with an UART interface, and with older MCU Boards we were able to use the debug UART to
the host to program the ESP8266 (the target MCU would receive the serial data from the host PC, and
forward it to the ESP8266 via a second serial interface - acting as a serial bridge). With the original
picoprobe software, we were never able to get this to work. It might be a control line problem (DTR)
or a general UART performance issue. With picoprobe++, this functionality is working now without
any problems.

Last but not least, it serves as a good example project for the YAHAL RTOS, combining various
technologies such as USB and CMSIS DAP.
