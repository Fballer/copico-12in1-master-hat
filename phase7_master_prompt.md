# Copico 12-in-1 Master Hat: Phase 7 (Wireless Suite) Master Prompt

## Role & Context
You are Antigravity, an expert embedded C++ AI coding assistant. We are building the firmware for the **Copico 12-in-1 Master Hat**, an expansion system for the vintage Tandy Color Computer (CoCo). 

We are starting **Phase 7: Wireless Suite**. To save tokens, we are beginning a fresh conversation. You must assimilate the following architecture, constraints, and progress to continue seamlessly.

## System Architecture
The system utilizes a dual-processor architecture:
1.  **Host Processor**: Raspberry Pi RP2350.
    *   **Core 1**: Runs a hyper-critical PIO bus sniffer (`coco_bus.pio`). It listens to the CoCo's 6809 CPU and must respond within **<500ns**. *Rule: Never introduce blocking delays, Mutexes, or heavy computation on Core 1.*
    *   **Core 0**: Handles the main loop, peripheral emulation (`tick()` functions), and I2S/VGA DMA management.
2.  **Network Coprocessor**: ESP32-C3. 
    *   Dedicated to handling TCP/IP, WiFi, Telnet, and FAT32 SD Card operations.

### The Interconnect (SPI)
The RP2350 and ESP32-C3 communicate exclusively over SPI. 
*   **RP2350 Pins**: `MISO` (23), `MOSI` (24), `CS` (26), `SCK` (28).
*   **Protocol**: Defined in `lib/shared/spi_protocol.h`. It relies on a fixed 264-byte packet (`SpiMasterPacket` / `SpiSlavePacket`) padded with a sync byte (0xAA) and a checksum.
*   **Current State**: We currently have two classes trying to use SPI: `Esp32Bridge` (synchronous, blocking polling used for SD card reads) and `SpiStream` (asynchronous, non-blocking tick-based stream meant for WiModem serial). *They currently share the same pins and need unification/careful management.*

## Current Project State (Phases 1-6 Complete)
*   **Chameleon BIOS**: We have completely written a 6809 assembly BIOS (`bios/chameleon.asm`) that lives in the RP2350 and gets injected into the CoCo's ROM space at `$C000-$DFFF`. It provides a native 32x16 UI to dynamically swap emulator modes (e.g., `MODE_WIMODEM`, `MODE_COCOSDC`, `MODE_ORCH90`).
*   **Shared BIOS Memory**: The BIOS reads real-time status strings from the RP2350 at specific memory addresses:
    *   `$C800`: WiFi Status String
    *   `$C820`: ESP32 Firmware Version String
    *   `$C840`: SD Card Status String
*   **Emulators Built**: We have functioning classes for `Acia6551` (RS-232), `Pic7040`/`Ay8913`/`Spo256` (Speech/Sound), `V9958` (VGA), and `Cocosdc` (SD floppy).

## Phase 7 Goals
The objective of Phase 7 is to bring the ESP32-C3 online and implement the **WiModem** (an RS-232 to Telnet bridge for BBSing).

### 1. The ESP32 Firmware (New Project)
You will need to help scaffold a brand new PlatformIO project for the ESP32-C3.
*   **Project Location**: Create the new project in `firmware/esp32-coprocessor`.
*   **Environment**: Use `board = esp32-c3-devkitm-1` (or equivalent C3 board) and `framework = arduino`.
*   **Reference**: We are NOT building the modem from scratch. You must reference the local project at `/Users/macbook/Documents/PlatformIO/Projects/New_Porta_Serial_esp32c3`. This contains a working implementation of `Zimodem` (an industry-standard AT command parser) previously built for the CoCo.
*   **Adaptation**: We must strip out the hardware UART dependencies in the `Zimodem` reference and pipe its I/O through our SPI Slave interface (`ESP32SPISlave` library) using the fixed 264-byte packets.

### 2. The RP2350 Firmware (Existing Project)
*   **Existing ACIA Work**: We have already completed the `Acia6551` class (during Phase 3). While it is pending final hardware verification, it supports both Legacy and Turbo modes (up to 115200 baud) and is designed to accept ANY Arduino `Stream` via dependency injection. **Do NOT rebuild or modify `acia6551.cpp` for no reason**.
*   **Bridging**: In `main.cpp`, the physical RS-232 Pak uses `Acia6551 acia(&rs232_serial);`. For the WiModem, we have already wired up `Acia6551 wimodem(&spi_wimodem_stream);`. When `current_mode == MODE_WIMODEM`, Core 1 successfully routes `$FF68-$FF6B` register access to this `wimodem` object.
*   **SpiStream**: Because `wimodem` takes the `SpiStream` pointer, your only task on the RP2350 side is to finalize `spi_stream.cpp`. You must ensure that bytes written to the ACIA are pushed into the SPI TX ring buffer, and then flushed to the ESP32 via our 264-byte SPI packets during Core 0's 5ms `tick()` cycle.

## Your Immediate Instructions
1. Acknowledge this context and confirm your understanding of the architecture.
2. Draft a step-by-step Execution Plan for Phase 7 based on this prompt. 
3. Stop and wait for my approval before modifying any files or executing terminal commands.
