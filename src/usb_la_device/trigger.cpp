///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#include "trigger.h"
#include "RP2040.h"

using namespace _IO_BANK0_;
using namespace _SIO_;

bool trigger::set(const trigger_type t, const uint8_t gpio) {
    // Edge triggers can only be set once!
    if (_trigger_type > 2) return false;
    _trigger_type = t;
    switch (t) {
        // State triggers use the GPIO_IN register in SIO, so we
        // can check up to 32 GPIO pins in parallel. The INTR
        // register would also be possible, but we can only check
        // 8 consecutive GPIOs with a single INTR register.
        case LEVEL_LOW: {
            _reg = reinterpret_cast<uint32_t *>(&SIO.GPIO_IN);
            _mask  |=   1 << gpio;
            _value &= ~(1 << gpio);
            break;
        }
        case LEVEL_HIGH: {
            _reg = reinterpret_cast<uint32_t *>(&SIO.GPIO_IN);
            _mask  |= 1 << gpio;
            _value |= 1 << gpio;
            break;
        }
        // Edge triggers use the INTR register in IO_BANK0
        case FALLING:
        case RISING:
        case EDGE: {
            _reg   = reinterpret_cast<uint32_t *>(&IO_BANK0.INTR0 + (gpio >> 3));
            _mask  = static_cast<uint32_t>(t) << ((gpio & 0x7) * 4);
            break;
        }
        case NONE: {
            break;
        }
    }
    return true;
}

bool trigger::is_set() const {
    return (_trigger_type != NONE);
}

void trigger::reset() {
    // Reset all values
    _trigger_type = NONE;
    _mask  = 0;
    _value = 0;
    _reg   = nullptr;
}

void COPY_TO_RAM trigger::wait(const volatile bool & abort_flag) const {
    // Fast exit if no trigger type was set
    if (_trigger_type == NONE) return;
    if (_trigger_type > LEVEL_HIGH) {
        // Reset edge triggers
        *_reg = _mask;
        // Loop until edge trigger matches
        while (((*_reg & _mask) == 0) && !abort_flag) { }
    } else {
        // Loop until state trigger matches
        while (((*_reg & _mask) != _value) && !abort_flag) { }
    }
}
