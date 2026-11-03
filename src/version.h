///////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Andreas Terstegge
//
// This file is part of picoprobe++
// A CMSIS-DAP v2 firmware for RP2040/RP2350 based debug probes
///////////////////////////////////////////////////////////////
//
#ifndef VERSION_H
#define VERSION_H

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_PATCH 0

#define VERSION_TO_STR(major, minor, patch) #major "." #minor "." #patch
#define VERSION_HELPER(major, minor, patch) VERSION_TO_STR(major, minor, patch)
#define VERSION_STRING  VERSION_HELPER(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)
#define VERSION_BCD     (VERSION_MAJOR << 8 | VERSION_MINOR << 4 | VERSION_PATCH)

#endif // VERSION_H
