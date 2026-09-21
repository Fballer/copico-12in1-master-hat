# Phase 9: CoPico 10-in-1 Multi Hat BIOS Finalization

Complete the branding overhaul, implement the "Setup Required" alert system, and add the "Flash ROM Management" and "Info" pages. Includes support for dual-path SDC-DOS loading (Root or `/ROMS/`).

## User Review Required

> [!IMPORTANT]
> **Screen Width & Row Limits**: The CoCo screen is 32x16. I have adjusted the spacing on the Info page to ensure all credits fit within the 16-row vertical limit.
> - **Detailed Backend Integration**: I have expanded Step 5 to show exactly how the RP2350 will "wire" the new BIOS commands.

---

## Proposed Changes

### 1. Main Menu Rebranding ([`xbios.asm`](file:///Users/macbook/Documents/PlatformIO/Projects/copico/copico-12in1-master-hat/firmware/master-hat-firmware/bios/xbios.asm))

- **Title**: `    COPICO 10 IN 1 MULTI HAT     `
- **Sub-Header**: `      CREATED BY PORTACOCO      ` (Row 1).
- **Settings Header**: `--------CURRENT SETTINGS--------`
- **Toggle Header**: `------PRESS KEY TO TOGGLE-------`
- **Footer Row 14**: ` PRESS [enter] TO RESTART COCO `
- **Footer Row 15**: ` PRESS [o] OPTIONS OR [i] INFO  `

### 2. "Setup Required" Alert Screen

Triggered on boot if `$D880` bit 0 is set.

- **Row 0 (dark)**: `*** SETUP REQUIRED ***`
- **Row 2 (dark)**: `-------- ACTION NEEDED ---------`
- **Row 4 (norm)**: `SDC-DOS ROM IS NOT INSTALLED.`
- **Row 6 (norm)**: `PLEASE PLACE COCOSDC.ROM IN`
- **Row 7 (norm)**: `ROOT OR /ROMS/ FOLDER ON SD`
- **Row 8 (norm)**: `CARD AND RESTART TO INSTALL`
- **Row 9 (norm)**: `SDC-DOS.`
- **Row 14 (dark)**: `--------------------------------`
- **Row 15 (toggl)**: `PRESS ANY KEY TO RESTART`

### 3. Flash ROM Management Page (Option 7)

- **Row 0 (dark)**: `*** FLASH ROM MANAGEMENT ***`
- **Row 2 (dark)**: `----  FLASH BANK STATUS  ----`
- **Row 3 (norm)**: `[SLOT 0] X-BIOS     : INSTALLED`
- **Row 4 (norm)**: `[SLOT 1] SDC-DOS    : (DYNAMIC)`
- **Row 5 (norm)**: `[SLOT 2] FUJINET    : INSTALLED`
- **Row 6 (norm)**: `[SLOT 3] RS-232 PAK : INSTALLED`
- **Row 8 (toggl)**: `[S] SCAN SD FOR ROMS`
- **Row 9 (toggl)**: `[C] CLEAR FLASH BANK`
- **Row 10 (toggl)**: `[R] RE-INSTALL DEFAULTS`
- **Row 11 (toggl)**: `[I] BOOT INTERNAL ROM (PASS-THRU)`
- **Row 13 (dark)**: `--------------------------------`
- **Row 15 (toggl)**: `[X] RETURN`

### 4. Info Page (Hotkey 'I')

- **Row 0 (dark)**: `*** COPICO 10 IN 1 MULTI HAT ***`
- **Row 3 (norm)**: `COPICO Designed by:`
- **Row 4 (norm)**: `HENRY STRICKLAND AND`
- **Row 5 (norm)**: `    THOMAS SHANKS`
- **Row 6 (norm)**: `GITHUB.COM/STRICKYAK/COPICO-BONOBO`
- **Row 7 (norm)**: `--------------------------------`
- **Row 8 (norm)**: `THIS HAT IS DESIGNED FOR THE`
- **Row 9 (norm)**: `  CENTIPEDE 32Z BOARD ONLY`
- **Row 10 (norm)**: `--------------------------------`
- **Row 11 (norm)**: `10 in 1 Hat Designed by Mark Pacan`
- **Row 12 (norm)**: `10 IN 1 HAT Software by Mark Pacan`
- **Row 13 (norm)**: `              www.portacoco.com`
- **Row 14 (norm)**: `--------------------------------`
- **Row 15 (toggl)**: `[X] RETURN`

### 5. C++ Backend Support Details ([`main.cpp`](file:///Users/macbook/Documents/PlatformIO/Projects/copico/copico-12in1-master-hat/firmware/master-hat-firmware/src/main.cpp))

To support the new BIOS utilities, we will implement a high-speed command trap in the Core 1 bus handler (`loop1`).

#### A. Command Register Trap
Add a trap for address `$FF74` in the `is_read == false` (write) block of `loop1()`:
```cpp
if (addr == 0xFF74) {
    handle_bios_command((uint8_t)data);
    return;
}
```

#### B. Command Logic Implementation
Implement `handle_bios_command(uint8_t cmd)` with the following behaviors:
1.  **`0x01` (Scan & Install SDC-DOS)**:
    - Attempt to open `COCOSDC.ROM` from the SD Root.
    - If not found, attempt to open `/ROMS/COCOSDC.ROM`.
    - If found, stream data into `flash_rom.install_rom(SLOT_COCOSDC, ...)` and clear the `FLASH_STATUS_SDC_MISSING` flag.
2.  **`0x02` (Clear Flash Bank)**:
    - Call `flash_rom.install_rom(SLOT_COCOSDC, nullptr, 0, ...)` with an empty buffer to erase the slot and set the missing flag.
3.  **`0x03` (Re-install Defaults)**:
    - Trigger `flash_rom.init_factory_defaults()` to restore Slot 0 (BIOS), Slot 2 (FujiNet), Slot 3 (RS-232), and Bank 1 (BASIC).
4.  **`0x09` (Internal ROM Boot)**:
    - Call `switch_mode(MODE_INTERNAL_ROM)`. This puts the Hat into high-impedance mode (tri-state) and triggers a software reset of the CoCo.

#### C. RTC Register Shift
Since `$FF74` is now the Command Port, move the RTC registers to `$FF75` and `$FF76` to avoid conflict, updating both `xbios.asm` and `io_dispatch.cpp` accordingly.

---

## Verification Plan

1. **Visual Check**: Verify the Info page text fits without overflowing.
2. **Path Test**: Verify `COCOSDC.ROM` installs from both Root and `/ROMS/`.
3. **Logic Check**: Verify all hotkeys and menu options function. Verify Option 7 -> `I` triggers a tri-state reset.
4. **Consistency Check**: Verify RTC functionality still works at its new address.
