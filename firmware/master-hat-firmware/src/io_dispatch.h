#pragma once
// ================================================================
// Hybrid Fast-Map I/O Dispatch Table
// ================================================================
// WHY: The CoCo bus at 2.86 MHz (GIME-X + 6309) gives us only
//      175 ns to respond to a read. The old if/else chain took
//      ~40 ns just in branch comparisons. This table replaces
//      that chain with a single indexed function-pointer lookup,
//      reducing dispatch to ~7 ns (1 load + 1 indirect call).
//
// HOW: A 256-entry table covers $FF00-$FFFF. Each entry is a
//      function pointer: uint8_t (*handler)(uint16_t addr).
//      NULL means "no device here" (open bus). The table is
//      rebuilt on every mode switch via rebuild_io_tables().
//
// ROM: The $C000-$DFFF ROM shadow is handled by a single fast
//      function pointer (rom_read_handler) checked before the
//      table lookup, since it spans 8KB and would waste memory
//      if stored as 8192 table entries.
// ================================================================

#include <Arduino.h>
#include "emulator_mode.h"

// Function pointer type for I/O read/write handlers.
// Read:  returns the byte to put on the bus.
// Write: returns 0 (ignored), data is passed via the global.
typedef uint8_t (*IoReadFunc)(uint16_t addr);
typedef void    (*IoWriteFunc)(uint16_t addr, uint8_t data);

// The ROM shadow handler — set per-mode, NULL if no ROM active.
typedef uint8_t (*RomReadFunc)(uint16_t addr);

// 256-entry tables for $FF00-$FFFF (indexed by addr & 0xFF)
extern IoReadFunc  io_read_table[256];
extern IoWriteFunc io_write_table[256];

// Single function pointer for ROM reads ($C000-$DFFF)
extern RomReadFunc rom_read_handler;

// Rebuild both tables + ROM handler for the given mode.
// Called from switch_mode() and once during setup().
void rebuild_io_tables(EmulatorMode mode);
