#ifndef EMULATOR_MODE_H
#define EMULATOR_MODE_H

enum EmulatorMode {
    MODE_BOOT_MENU,           // Chameleon BIOS (Slot 0) — always in flash
    MODE_ORCH90,              // Orchestra-90 stereo audio
    MODE_SPEECH_SOUND,        // Speech/Sound Pak
    MODE_RS232_PAK_LEGACY,    // RS-232 Pak, 2400 baud
    MODE_RS232_PAK_TURBO,     // RS-232 Pak, 115200 baud
    MODE_WORDPAK2,            // WordPak II+ 80-col VGA
    MODE_COCOSDC,             // CoCoSDC disk emulation (SD card required)
    MODE_FUJINET,             // FujiNet BIOS (Slot 2)
    MODE_WIMODEM,             // WiModem (WiFi modem)
    MODE_INTERNAL_ROM,        // Pass-through: Hat tri-states, CoCo uses its own ROMs
    MODE_HAT_ACTIVE,          // Layered hat config running (audio+disk+comm stack)
    MODE_MAX                  // Sentinel — must always be last
};

#endif // EMULATOR_MODE_H
