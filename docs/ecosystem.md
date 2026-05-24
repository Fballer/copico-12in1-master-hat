# CoPico project ecosystem

This repo (`copico-12in1-master-hat`) is the **shipping firmware** for the PortaCoco 10-in-1 Centipede 32z hat. It sits inside a larger folder on disk that you should open as one workspace when developing.

## Recommended Cursor / IDE workspace

**Option A (best):** Open `copico.code-workspace` in this repo — it includes this firmware, `lwtools-4.21`, and `copico-bonobo-main` as multi-root folders. Bonobo KiCad/Gerbers are excluded from search/index via `copico-bonobo-main/.cursorignore` and workspace `search.exclude` (keeps `centipede-watcher/`, `references/`, and `.md` files).

**Option B:** Open the parent folder:

```
/Users/macbook/Documents/PlatformIO/Projects/copico/
```

That gives agents and search access to build tools and reference material without copying them into git.

## Sibling folders (under `copico/`)

| Path | Purpose |
|------|---------|
| **`copico-12in1-master-hat/`** (this repo) | RP2350 + ESP32 firmware, docs, `references/` |
| **`lwtools-4.21/`** | 6809 assembler (`lwasm`) for CoPico X-BIOS — used by `firmware/master-hat-firmware/bios/build_bios.py` |
| **`copico-bonobo-main/`** | Centipede 32z/32d KiCad, `centipede-watcher` bus-sniffer experiments, extra `hat-fuji-40c/references/` |
| **`hardware/`** | Production Gerbers, schematic PDFs, PCB archives (not source of truth for day-to-day wiring) |

## Authoritative hardware docs (this repo)

| Doc | Use |
|-----|-----|
| [schematic_copico10in1-fixed_2026-05-24.png](schematic_copico10in1-fixed_2026-05-24.png) | **Wiring you built** — PortaCoco 10-in-1 hat (rev 2.12) |
| [pinmap.md](pinmap.md) | GPIO ↔ net names ↔ firmware `#define`s |
| [build_guide.md](build_guide.md) | Assembly BOM and J3/J4 steps (verified section only) |

Ignore `hardware/` in this repo (excluded via `.cursorignore`) — old FujiNet-era KiCad, not the Centipede hat you assembled. Use `docs/schematic_copico10in1-fixed_2026-05-24.png` instead.

## Build commands

```bash
# RP2350 (default)
cd firmware/master-hat-firmware && pio run -e rp2350

# ESP32 coprocessor (from same project)
pio run -e esp32c3_coprocessor

# CoPico X-BIOS ROM (needs lwasm — see lwtools-4.21 sibling)
cd firmware/master-hat-firmware/bios && python3 build_bios.py
```

## Centipede base board (CoCo bus)

The hat schematic covers J3/J4 peripherals. The **6809 bus** pins are on the Centipede 32z itself:

| CoCo signal | RP2350 GPIO | Firmware |
|-------------|-------------|----------|
| D0–D7 | 0–7 | `coco_bus.pio` |
| A0–A15 | 32–47 | `coco_bus.pio` |
| R/W | 20 | `PIN_RW`, PIO jmp pin |
| E-clock | 21 | `PIN_E_CLOCK` |

See `copico-bonobo-main/v3.1-centipede/README.md` for the full Centipede pin table (Q, NMI, RESET, etc.).

## Git

Only `copico-12in1-master-hat` is a git repository. Siblings are local support files — do not expect them on GitHub clones unless you copy `lwtools-4.21` yourself or install `lwasm` on PATH.
