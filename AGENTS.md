# Agent instructions — CoPico 12-in-1 Master Hat

## Workspace

Prefer opening **`/Users/macbook/Documents/PlatformIO/Projects/copico/`** (parent folder), not only this repo. Sibling **`lwtools-4.21/`** is required to rebuild the X-BIOS ROM.

Read **`docs/ecosystem.md`** first for layout and dependencies.

## Hardware truth

- **Wiring / GPIO**: `docs/schematic_copico10in1-fixed_2026-05-24.png`, `docs/pinmap.md`, `docs/build_guide.md` (top section only).
- **Ignore** `hardware/` in this repo (`.cursorignore` — old FujiNet KiCad).
- **CoCo bus** (D0–7, A0–15, R/W, E): Centipede 32z — see `docs/pinmap.md` and `copico-bonobo-main/v3.1-centipede/README.md`.

## Firmware entry points

| Target | Path | Env |
|--------|------|-----|
| RP2350 | `firmware/master-hat-firmware/` | `pio run -e rp2350` |
| ESP32-C3 | same project | `pio run -e esp32c3_coprocessor` |
| X-BIOS ROM | `firmware/master-hat-firmware/bios/` | `python3 build_bios.py` |

Core logic: `src/main.cpp` (dual-core), `src/coco_bus.pio`, `src/io_dispatch.cpp`, `src/esp32_bridge.cpp`, `lib/shared/spi_protocol.h`.

## Local specs (use before web search)

- `references/peripherals_specs.md`
- `references/wordpak2/v9958_specs.md`
- `references/cocosdc/cocosdc_specs.md`

## Do not

- Change GPIO assignments without checking `docs/pinmap.md` and firmware `#define`s.
- Assume MicroSD is on the RP2350 — it is on the ESP32 (GPIO 5–7, 10).
- Commit secrets or `.pio/` build artifacts.
