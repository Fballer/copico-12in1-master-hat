# Phase 10: Hardware Bring-Up & Testing

## Current State
- The physical PCBs have arrived and are fully assembled.
- The firmware for all core emulations (Speech/Sound, RS-232, WordPak, CoCoSDC, WiModem, Boot Menu) has been written and successfully compiles.
- The Phase 9 BIOS implementation plan for rebranding and SD Card dual-path has been completed (though code execution is pending).

## Objectives for Phase 10
1. **Initial Hardware Validation**: Safely test the newly assembled PCB's power rails (5V, 3.3V) and basic physical inputs (Dual-action button, Status LED) before plugging it into the CoCo.
2. **Peripheral Smoke Tests**: Test standalone components like the PCM5102A I2S DAC and the MAX3232 RS-232 level shifters to confirm soldering integrity.
3. **Bus Sniffer Validation**: Insert the Hat into the CoCo and run minimal PIO test scripts to verify the RP2350 can correctly sniff the address bus and E-Clock.
4. **Integration Testing**: Once the hardware is proven stable, we will execute the Phase 9 BIOS firmware changes, flash the master firmware, and begin testing the fully integrated system on real hardware.

## How to Proceed
Please assist me with writing minimal Arduino test sketches (e.g., LED blinker, I2S tone generator, PIO address logger) to validate the physical hardware step-by-step. Let me know if you need schematic or pinout references during this process.
