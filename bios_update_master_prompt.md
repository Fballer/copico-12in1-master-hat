# Master Task: Finalizing the CoPico X-BIOS (formerly Chameleon)

## 🎯 Objective
Update the 6809 assembly-based BIOS for the **Copico 12-in-1 Master Hat (RP2350 + ESP32-C3)**. The goal is to implement a robust "Setup Required" alert system for missing ROMs and a "Utility Page" for managing the new Flash ROM Bank architecture.

---

## 🏗️ System Architecture
- **Master Controller**: RP2350 (ARM Cortex-M33 @ 150MHz).
- **Coprocessor**: ESP32-C3 (Handles WiFi, SD Card, and Bluetooth).
- **Host**: Tandy Color Computer (CoCo) 1, 2, or 3.
- **Interconnect**:
    - **RP2350 <-> CoCo**: Parallel bus sniffer via PIO (Address: G32-G47, Data: G0-G7).
    - **RP2350 <-> ESP32**: High-speed SPI (G23/G24/G30/G31).
    - **Video**: 640x480 VGA (PIO/DMA driven).

---

## 💾 Flash ROM Bank Layout (256KB @ 0x3C0000)
The RP2350 uses its internal flash to store ROM images in 16KB slots.
- **Slot 0**: CoPico X-BIOS (Pre-loaded).
- **Slot 1**: SDC-DOS (Auto-installed from SD card).
- **Slot 2**: FujiNet BIOS (Pre-loaded/Downloadable).
- **Slot 3**: RS-232 Pak ROM (Pre-loaded).
- **Banks 0-7**: CoCoSDC Array (Bank 1 is pre-loaded with Disk BASIC 1.1).

---

## 🖥️ Shared Memory Interface ($C000 - $DFFF)
The BIOS runs in the CoCo's address space. The RP2350 serves data to the BIOS and monitors its state via specific memory addresses:
- **$C000 - $D7FF**: Main BIOS Binary (8KB).
- **$D800 - $D81F**: WiFi Status String (from ESP32).
- **$D820 - $D83F**: Firmware Version String.
- **$D840 - $D85F**: SD Card Status String.
- **$D880**: **FLASH_STATUS_BYTE** (Bit 0: 1=SDC-DOS Missing, 0=OK).
- **$FF70 - $FF73**: Hardware Config Registers (Audio, Video, Disk, Comm).
- **$FF74**: **MODE_SWITCH_CMD** (Write a mode ID here to trigger a hardware swap).

---

## ⚡ Hardware Pinout (Verified PCB Netlist)
- **PIN_BTN_DUAL (G19)**: Held at boot forces CoPico X-BIOS (Slot 0).
- **PIN_STATUS_LED (G17)**: System status.
- **VGA Sync**: H-Sync (G26), V-Sync (G27).
- **VGA RGB**: G18, G22, G28 (mapped via PIO).
- **I2S DAC**: BCK (G10), DIN (G11), LCK (G12).

---

## 🛠️ BIOS Requirements (6809 Assembly)
1. **Name Change**: Rename all UI labels and internal strings to **CoPico X-BIOS**.
2. **Startup Alert**: On boot, check `$D880`. If Bit 0 is set, display a large, detailed "SETUP REQUIRED" screen.
    - Message: "Please place COCOSDC.ROM in /ROMS/ folder on SD card and restart to install SDC-DOS."
3. **ROM Utility Page**:
    - Add a new menu page: `Flash ROM Management`.
    - Options: `Scan SD for ROMs`, `Clear Flash Bank`, `Re-install Defaults`.
4. **Hardware Pass-through**: Implement an option for `MODE_INTERNAL_ROM` which tri-states the Hat and lets the CoCo boot its internal ROMs.

---

## 📂 Current Progress
- C++ Flash Manager is complete and pushed to `phase8-rtc`.
- SPI timeouts and Jitter-free VGA are implemented.
- `switch_mode()` now handles 16KB flash loads and tri-state recovery.
- Project renamed to **CoPico X-BIOS**.

**Please begin by reviewing `firmware/master-hat-firmware/bios/xbios.asm` and proposing the UI update plan.**
