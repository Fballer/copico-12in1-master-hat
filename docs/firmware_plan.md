# Copico 10-in-1 Master Hat: Firmware Architecture & Implementation Plan
**Date: May 6, 2026**

This document details the software architecture and development roadmap for the **10-in-1 Master Hat**. It leverages the RP2350 dual-core processor to emulate an entire suite of CoCo expansions.

> [!IMPORTANT]
> This plan assumes the hardware is built according to the [Hardware Assembly Guide](file:///Users/macbook/.gemini/antigravity/brain/01be4bfb-ce62-4b38-8036-e57a2e46b2b8/hardware_build_guide.md).

## 1. System Philosophy: The "Chameleon" Expansion
The RP2350 acts as a real-time bridge between the CoCo's 6809 bus and modern peripherals. By dynamically swapping PIO state machines and memory maps, a single physical board becomes 10 different classic cartridges.

> [!IMPORTANT]
> **LOCAL SPECIFICATIONS FIRST**: The technical "guts" (registers, ports, and protocols) for these modules have been saved directly to this project. **Reference these local files first** during the firmware build to ensure we stay offline and fast.

| Emulation Mode | Core Hardware | Technical Reference / Repository |
| :--- | :--- | :--- |
| **Orchestra-90** | 8-bit Mono DAC | [Archive: Orch-90 Manual](https://colorcomputerarchive.com/repo/Documents/Manuals/Hardware/Orchestra-90%20(Tandy).pdf) \| **[LOCAL: Peripherals Specs](file:///Users/macbook/Documents/PlatformIO/Projects/copico/copico-bonobo-main/v3.1-centipede/hat-fuji-40c/references/peripherals_specs.md)** |
| **Speech & Sound** | AY-3-8910 (PSG) | [GitHub: AY-3-8910 Emulation](https://github.com/mamedev/mame/blob/master/src/devices/sound/ay8910.cpp) \| [GitHub: LibAYemu](https://github.com/vsergeev/libayemu) |
| **RS-232 Pak** | ACIA 6551 | [Reference: 6551 Datasheet](https://www.westerndesigncenter.com/wdc/documentation/w65c51n.pdf) \| **[LOCAL: Peripherals Specs](file:///Users/macbook/Documents/PlatformIO/Projects/copico/copico-bonobo-main/v3.1-centipede/hat-fuji-40c/references/peripherals_specs.md)** |
| **WordPak 2+** | Yamaha V9958 | [Source: CoCoLoCo WordPak-2](https://sites.google.com/site/tandycocoloco/wordpak-2) \| **[LOCAL: V9958 Specs](file:///Users/macbook/Documents/PlatformIO/Projects/copico/copico-bonobo-main/v3.1-centipede/hat-fuji-40c/references/wordpak2/v9958_specs.md)** |
| **CoCoSDC** | Custom FDC | [Blog: CoCoSDC Official](http://cocosdc.blogspot.com/) \| **[LOCAL: CoCoSDC Specs](file:///Users/macbook/Documents/PlatformIO/Projects/copico/copico-bonobo-main/v3.1-centipede/hat-fuji-40c/references/cocosdc/cocosdc_specs.md)** |
| **FujiNet** | ESP32 Networking | [GitHub: FujiNet PlatformIO](https://github.com/FujiNetWIFI/fujinet-platformio) \| [GitHub: fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware) |
| **DriveWire 4** | Virtual Serial Disk | [Source: DriveWire 4](http://www.drivewire4.com/) \| [GitHub: pyDriveWire](https://github.com/n64764/pyDriveWire) |
| **WiModem** | AT Command Modem | [Reference: Zimodem Firmware](https://github.com/boisy/Zimodem) \| [GitHub: GuruModem](https://github.com/pizet/GuruModem) |
| **Real-Time Clock** | MSM5832 Emulation | [Library: DS3231 I2C](https://github.com/adafruit/Adafruit_DS3231) \| [GitHub: CoCo RTC Driver](https://github.com/boisy/NitrOS-9/blob/master/sys/modules/rtc3231.as) |
| **SuperSprite FM+** | TMS9918 + OPL2 | [Reference: TMS9918 Datasheet](https://www.msxarchive.nl/pub/msx/mirrors/hansa.xs4all.nl/tms9918.pdf) \| [GitHub: OPL2 Emulator](https://github.com/D-S-X/OPL2-Emulation) |
| **Lark MIDI Pak** | 6850 ACIA | [Reference: MIDI 1.0 Spec](https://www.midi.org/specifications-old/item/the-midi-1-0-specification) \| Emulates hardware MIDI through the 4-pin expansion header. |
| **Boot Menu** | Injected 6809 Code | [Source: FujiNet Config Application](https://github.com/FujiNetWIFI/fujinet-config) \| [GitHub: Mega-Cart](https://github.com/sublogic/MegaCart) |

---

## 3. Development Roadmap & Success Probability

We will develop each module in isolation, hardcoding the RP2350 for testing before integrating them into the master Boot Menu.

### Phase 1: Orchestra-90 Emulation (Success: 95%)
*   Trap `$FF7A/B` and push to I2S DAC. Simple bus-sniffing "quick win."

### Phase 2: Speech and Sound Pak (Success: 85%)
*   Real-time synthesis of PSG waveforms in the RP2350 CPU core.
*   **CoCo 3 / 6309 / GIME-X Auto-Correction**: Detect host E-Clock frequency in real-time (0.89MHz to 3MHz+). Normalizes PSG pitch and ensures bus-trapping logic remains stable during high-speed operation.

### Phase 3: RS-232 Pak (Success: 90%)
*   Emulate ACIA registers and route to physical MAX3232 pins.

### Phase 4: WordPak 2+ (V9958 VGA) (Success: 75%)
*   **The "Boss Fight"**: Emulate MSX2+ graphics and output 3-bit RGB VGA. Requires intense PIO optimization. Master this first to provide the video foundation for the SuperSprite.

### Phase 5: The Native Boot Menu (Success: 85%)
*   Inject 6809 assembly into CoCo RAM to display a selection menu on the native screen.

### Phase 6: CoCoSDC (Local Storage) (Success: 80%)
*   Implement FAT32 support and precise floppy controller timing.

### Phase 7: Wireless Suite (ESP32-C3)
*   **7A. WiFi Connection** (95%)
*   **7B. Wireless WiModem** (90%): Bridge RS-232 to Telnet via ESP32.
*   **7C. FujiNet / DriveWire** (80%): Stream sectors over the local network.
*   **7D. Cloud CoCoSDC** (75%): Fetch `.DSK` files directly from the Color Computer Archive.

### Phase 8: SuperSprite FM+ (Success: 70%)
*   **Advanced Feature**: Leverage the V9958 VGA core from Phase 4 and add software FM synthesis (OPL2 emulation). This turns the CoCo into an FM-synth music machine.

---

## 4. Boot Menu & ROM Management
*   **Native VDG Menu**: The primary menu runs on the CoCo's original green/black screen.
*   **BIOS Shared Memory**: The RP2350 exposes a shared memory block starting at `$C800` for the 6809 to read real-time status strings dynamically injected by the ESP32 and RP2350:
    *   `$C800`: WiFi Status (max 32 bytes, null-terminated)
    *   `$C820`: ESP32 Firmware Version (max 32 bytes, null-terminated)
    *   `$C840`: SD Card Status (max 32 bytes, null-terminated)
    *   `$C860`: Time Zone Setting (max 32 bytes, null-terminated)
*   **Hardware Registers**:
    *   `$FF50-$FF51`: Emulated MSM5832 RTC interface (compatible with Disto 4-N-1 and NitrOS-9).
    *   `$FF75`: Time Zone control register (Write 0-4 to configure offset from ESP32 NTP).
*   **WiFi Soft-Reset**: On boot/reset, the RP2350 sends a high-priority "System Reset" command via SPI to the ESP32 to ensure synchronized initialization (Software Handshake).
*   **Personality Swapping**: Selecting a menu item triggers a "hot-swap" of the RP2350's memory mapping.

### ROM Management Strategy (SD Card Loading)
The Boot Menu will provide specific ROM options for the $C000-$DFFF address space, depending on the selected hardware emulation:
1.  **Slot 1: Standard RSDOS**: (Hardcoded). Used only if passing through to physical hardware.
2.  **Slot 2: SDC DOS**: (Hardcoded). Heavily modified disk ROM required for the CoCoSDC emulation.
3.  **Slot 3: HDB-DOS**: (Hardcoded). Required for DriveWire and FujiNet wireless disk emulation.
4.  **Slot 4: Custom User ROM**: The menu will scan a `/ROMS/` folder on the MicroSD card. Users can drop any `.rom` file (e.g., NitrOS-9 boot ROMs, diagnostics) into this folder via their PC, and the RP2350 will dynamically load it into emulation memory.

### Multi-Pak Interface (MPI) Compatibility
While the 10-in-1 Hat cannot connect directly to a physical floppy drive, it **can** operate inside a Tandy Multi-Pak Interface alongside a real physical Disk Controller. 
*   **Hardware Handshake**: The Hat relies on the `CTS*` (Cartridge Select) signal. When in an MPI, the MPI hardware only sends the `CTS*` signal to the currently selected slot. 
*   **Result**: The Copico will remain completely "silent" on the bus until the CoCo switches to its specific MPI slot, allowing it to perfectly co-exist with real floppy controllers or other physical cartridges without bus conflicts.

*   **Physical Override**: The SPDT switch (G20) allows forcing a "Safe Mode" boot to the menu if a firmware setting gets stuck.
