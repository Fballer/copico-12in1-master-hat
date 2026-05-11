# Phase 7: Wireless Suite (ESP32-C3) - AI Prompt

**Role:** You are Antigravity, an expert embedded C++ AI coding assistant. We are building the firmware for the Copico 12-in-1 Master Hat, an expansion system for the Tandy Color Computer (CoCo) that uses an RP2350 (Host) and an ESP32-C3 (Network Coprocessor).

**Objective:** Begin Phase 7 (Wireless Suite). The goal is to integrate the ESP32-C3 over SPI to provide the CoCo with WiFi, a WiModem (RS-232 to Telnet bridge), DriveWire/FujiNet network disk streaming, and Cloud CoCoSDC capabilities.

### System Context & Constraints (Phases 1-6 Completed)
You are building on top of a highly integrated, stable codebase. You must ensure new code does not break existing functionality.
1.  **Architecture**: Dual-Core RP2350. Core 1 runs a time-critical PIO bus sniffer (`coco_bus.pio`) that responds to the 6809 CPU in <500ns. Core 0 handles high-level emulation loops (`tick()` functions).
2.  **Current Modes**: We have successfully implemented `MODE_ORCH90`, `MODE_SPEECH_SOUND`, `MODE_RS232_PAK_LEGACY`/`TURBO`, `MODE_WORDPAK2` (VGA), and `MODE_COCOSDC` (SD Card FAT32).
3.  **Hardware Resources (CRITICAL)**:
    *   **PIO0**: 2 SMs used by the Bus Sniffer.
    *   **PIO1**: 3 SMs used by the VGA Driver.
    *   **DMA**: VGA and I2S audio use active DMA channels.
    *   *Do NOT reallocate these resources or introduce blocking delays in Core 1.*
4.  **ESP32-C3 Hardware Mapping**:
    *   The ESP32 communicates with the RP2350 via SPI on these RP2350 pins: `MISO` (G23), `MOSI` (G24), `CS` (G26), `SCK` (G28).

### Phase 7 Requirements & Strategy
1.  **Do NOT Build From Scratch**: Leverage existing open-source projects to save time and ensure stability.
    *   **WiModem (7B)**: Reference `Zimodem` (boisy/Zimodem) or `GuruModem` for handling AT commands and Telnet bridging.
        *   **CRITICAL REFERENCE**: Use the local project [New_Porta_Serial_esp32c3](file:///Users/macbook/Documents/PlatformIO/Projects/New_Porta_Serial_esp32c3) as a primary guide. It contains a working WiModem setup previously built for the CoCo using the same ESP32-C3 processor.
    *   **Network Drives (7C/7D)**: Reference the `FujiNet` firmware stack (FujiNetWIFI) for SPI-to-Network bridging, or `pyDriveWire` specs for the DriveWire protocol.
2.  **Dual-Firmware Architecture**: We will need to write/adapt firmware for **both** the RP2350 (to marshal data between the CoCo bus and SPI) and the ESP32-C3 (to handle the TCP/IP stack and AT commands).
3.  **Integration**: The RP2350 logic must fit into our existing "Chameleon" architecture. For example, when `current_mode = MODE_WIMODEM`, Core 1 should trap the ACIA 6551 registers (like in Phase 3) but route the data to the ESP32 via SPI instead of a local hardware UART.

### Your First Task:
1.  Analyze this prompt and the current state of the workspace (specifically `main.cpp` and `docs/firmware_plan.md`).
2.  Do NOT write C++ code yet. 
3.  Draft a detailed `implementation_plan.md` for Phase 7. 
    *   Outline the SPI communication protocol we will use between the RP2350 and ESP32.
    *   Specify which open-source libraries we will pull in for the ESP32 firmware.
    *   Detail how we will bridge the existing `Acia6551` class to the new WiFi modem.
4.  Stop and wait for my approval before modifying source files.
