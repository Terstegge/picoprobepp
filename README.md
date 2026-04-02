# picoprobe++

### A debugger firmware for RP2040 and RP2350 based CMSIS-DAP v2 debug probes.

picoprobe++ is a CMSIS DAP v2.0 compatible debugger firmware written in C++.
It has most of the features and at least the performance of the original
Raspberry Pi [debugprobe](https://github.com/raspberrypi/debugprobe) software,
so it can be used as a replaycement if needed. picoprobe++ is not using the
Raspberry Pi pico SDK, tinyusb or the ARM CMSIS DAP implementation. Instead,
it is based on the following components:

* [YAHAL RTOS](https://git.fh-aachen.de/yahal/YAHAL),
  an embedded operating system written in C++, supporting ARM and ESP MCUs
* [tinyUSB++](https://github.com/Terstegge/tinyUSBpp), a well-structured
  USB library written in C++
* [openDAP++](https://github.com/Terstegge/openDAPpp), a portable CMSIS-DAP
  implementation written in C++
  
picoprobe++ benefits from the features of the libraries it uses.

* All needed USB descriptors in picoprobe++ are set up during runtime, which is
  a nice feature of tinyUSB++. So e.g. adding a second CDC ACM device for firmware
  debugging purposes is only 2 lines of code. tinyUSB++ also has full support for
  Microsofts OS 2.0 USB descriptor tree. This makes configuration changes in the
  USB descriptors very easy.
* picoprobe++ supports SWD and JTAG, which is a feature of openDAP++. However, SWD
  is by far the most commonly used debug protocol today, so JTAG is of less importance.
* picoprobe++ provides different low level DAP drivers. There is one driver exclusively
  using simple GPIO input/output, and a second driver using the PIO module of the RP2040/RP2350
  (which yield better performace, of course).
  This could be important in cases where the PIO modules are already used for someting else.

---
### Configuration

* Select board in CMake file
* modify config file
* LED file / class
* tbd

---
### Debugging the firmware

* Using UART
* Using additionl CDC ACM device
* log levels
* tbd

---
### Installation
To compile and install this Software, you need
* an installed [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads),
  (the arm-none-eabi variant)
* cmake
* a RP2040 or RP2350 based board, e.g. any kind of
 [Raspberry Pi Pico](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html) or a
 [Raspberry Pi Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html),
 or a board with an embedded debugger like the
 [RP2350 Launchpad](https://github.com/Terstegge/rp2350b-launchpad-PCB)
  
Most of the people being interested in this project will probaly have all that already.
Make sure you clone this repository recursively with all submodules (YAHAL is a submodule, which
itself contains submodules like openDAP++ or tinyUSB++:

`git clone https://github.com/Terstegge/picoprobepp.git --recursive`

Then procees with the typical cmake build steps:

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
forward it to the ESP8266 via a second serial interface - like a serial bridge). With the original
picoprobe software, we were never able to get this to work. It might be a control line problem (DTR)
or a general UART performance issue. With picoprobe++, this functionality is working now without
any problems.

Last but not least, it serves as a great example project for the YAHAL RTOS, combining various
technologies such as USB and CMSIS DAP. And of course it was great fun to realize it.

