///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
/**
 * @file trigger.h
 * @brief GPIO Trigger class to be used with a logic analyzer.
 *
 * This class implements a 'Trigger' - an object which will wait
 * until a specific GPIO status (levels or edges) are detected.
 */
#ifndef TRIGGER_H
#define TRIGGER_H

#include <cstdint>
#include "board.h"

/**
 * @brief Enum defining the various triggers. The values of the enum
 * items are selected according to the bits in the RP2040 INTR register.
 * So it is possible to uses the integer enum value as a bit mask when
 * reading/writing the INTR register.
 */
enum trigger_type {
    NONE=0, LEVEL_LOW=1, LEVEL_HIGH=2, FALLING=4, RISING=8, EDGE=12
};

class trigger {
public:
    /**
     * @brief Configure a trigger object by specifying the trigger
     * type and the GPIO pin which should be checked.
     * Multiple calls on the same trigger object with different GPIO
     * pins are possible in case of state triggers. The different
     * state triggers are then combined with AND logic, and the
     * trigger will only fire when all configured states are valid
     * at the same point in time. Edge triggers can not be combined
     * with other triggers.
     * @param t: Trigger type
     * @param gpio: The GPIO pin in the range of 0..31
     * @return true if method was successful, false otherwise
     */
    bool set(trigger_type t, uint8_t gpio);

    /**
     * @brief: Check if trigger is set
     * @return true is trigger is set
     */
    bool is_set() const;

    /**
     * @brief  Reset a trigger to its inactive (default) state.
     */
    void reset();

    /**
     * @brief Wait until the trigger fires. The method is moved
     * to RAM so we have a deterministic timing.
     * @param abort_flag If true, will terminate the wait process
     */
    void COPY_TO_RAM wait(const volatile bool & abort_flag) const;

private:
    trigger_type        _trigger_type {NONE};
    uint32_t            _mask  {0};
    uint32_t            _value {0};
    volatile uint32_t * _reg   {nullptr};
};

#endif // TRIGGER_H
