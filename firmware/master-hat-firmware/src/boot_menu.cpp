#include "boot_menu.h"

#include <string.h>
#include "../bios/chameleon_rom.h"

BootMenu::BootMenu() {
    memset(wifi_status, 0, sizeof(wifi_status));
    memset(fw_version, 0, sizeof(fw_version));
    memset(sd_status, 0, sizeof(sd_status));
    memset(tz_status, 0, sizeof(tz_status));
}

void BootMenu::init() {
    // Any specific initialization for the Boot Menu mode (if needed)
}

void BootMenu::update_sys_info(const char* wifi, const char* fw, const char* sd, const char* tz) {
    strncpy(wifi_status, wifi, sizeof(wifi_status) - 1);
    strncpy(fw_version, fw, sizeof(fw_version) - 1);
    strncpy(sd_status, sd, sizeof(sd_status) - 1);
    strncpy(tz_status, tz, sizeof(tz_status) - 1);
}

uint8_t BootMenu::read_rom(uint16_t address) {
    if (address >= 0xC000 && address <= 0xDFFF) {
        // Intercept Status Strings
        if (address >= 0xD800 && address < 0xD820) {
            return wifi_status[address - 0xD800];
        }
        if (address >= 0xD820 && address < 0xD840) {
            return fw_version[address - 0xD820];
        }
        if (address >= 0xD840 && address < 0xD860) {
            return sd_status[address - 0xD840];
        }
        if (address >= 0xD860 && address < 0xD880) {
            return tz_status[address - 0xD860];
        }

        // Return from auto-generated ROM array
        return chameleon_bios_bin[address - 0xC000];
    }
    return 0xFF; // Default open bus
}

void BootMenu::set_config(uint16_t address, uint8_t data) {
    if (address == 0xFF70) reg_audio = data;
    else if (address == 0xFF71) reg_video = data;
    else if (address == 0xFF72) reg_disk = data;
    else if (address == 0xFF73) reg_comm = data;
}

EmulatorMode BootMenu::calculate_mode() {
    // If Audio is Orch-90 or Speech/Sound, that takes over the system
    if (reg_audio == 1) return MODE_ORCH90;
    if (reg_audio == 2) return MODE_SPEECH_SOUND;

    // Video has next priority
    if (reg_video == 1) return MODE_WORDPAK2;

    // Disk/Comm have next priority
    if (reg_disk == 1) return MODE_COCOSDC;
    if (reg_disk == 2 || reg_comm == 2) return MODE_FUJINET; // FujiNet takes both
    if (reg_comm == 1) return MODE_RS232_PAK_LEGACY;
    if (reg_comm == 3) return MODE_WIMODEM;

    // Default fallback if everything is Regular CoCo (0)
    return MODE_COCOSDC;
}
