# Copico 12-in-1 Master Hat

The **Copico 12-in-1 Master Hat** is the ultimate expansion for the Tandy Color Computer. It emulates 12 legendary pieces of CoCo hardware using the high-performance Raspberry Pi RP2350 and an ESP32-C3 for wireless connectivity.

## 🚀 Emulated Hardware Features
1.  **Orchestra-90 CC**: 8-bit Stereo Audio.
2.  **Speech & Sound Pak**: PSG sound with automatic CoCo 3 pitch correction.
3.  **RS-232 Pak**: ACIA serial communication.
4.  **WordPak 2+**: MSX2+ (V9958) 80-column VGA graphics.
5.  **CoCoSDC**: High-speed MicroSD storage and disk emulation.
6.  **WiModem**: AT-command compatible wireless modem.
7.  **DriveWire 4**: Wireless virtual serial disk streaming.
8.  **FujiNet**: Cloud-based sector streaming and network tools.
9.  **Real-Time Clock**: DS3231-based system clock.
10. **SuperSprite FM+**: TMS9918 video + OPL2 FM synthesis sound.
11. **CoPico X-BIOS**: Software-based personality swapping via Native VDG menu.

## 📂 Repository Structure
*   `hardware/`: KiCad project files and Gerber archives.
*   `firmware/`: Source code for RP2350 (PIO-based bus trapping) and ESP32-C3.
*   `docs/`: Hardware assembly instructions and firmware roadmap.
*   `references/`: Technical specifications for the emulated hardware.

## 🛠 Documentation
*   [Hardware Build Guide](docs/build_guide.md)
*   [Firmware Architecture Plan](docs/firmware_plan.md)

---
*Created with AntiGravity - May 2026*
