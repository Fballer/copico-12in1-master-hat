#include "boot_menu.h"
#include <string.h>
#include "../bios/xbios_rom.h"
#include "flash_rom_manager.h"

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

        // Flash ROM status byte ($D880) — read by CoPico X-BIOS
        // to trigger "Setup Required" messages for missing ROMs.
        if (address == 0xD880) {
            return flash_rom.status_flags;
        }

        if (address == 0xD881) {
            return flash_result;
        }

        // Return from auto-generated ROM array
        return copico_xbios_bin[address - 0xC000];
    }
    return 0xFF; // Default open bus
}

void BootMenu::set_config(uint16_t address, uint8_t data) {
    if (address == 0xFF70) reg_audio = data;
    else if (address == 0xFF71) reg_video = data;
    else if (address == 0xFF72) reg_disk = data;
    else if (address == 0xFF73) reg_comm = data;
}

void BootMenu::set_flash_command(uint8_t cmd) {
    if ((cmd >= FLASH_CMD_SCAN_SD && cmd <= FLASH_CMD_REINSTALL) ||
        cmd == FLASH_CMD_INTERNAL_ROM) {
        pending_flash_cmd = cmd;
        flash_result = FLASH_RESULT_BUSY;
    }
}

void BootMenu::run_flash_command(uint8_t cmd) {
    bool ok = false;

    switch (cmd) {
        case FLASH_CMD_SCAN_SD:
            ok = flash_rom.install_cocosdc_from_sd();
            break;
        case FLASH_CMD_CLEAR_BANK:
            flash_rom.clear_user_slots();
            ok = true;
            break;
        case FLASH_CMD_REINSTALL:
            flash_rom.init_factory_defaults(true);
            if (flash_rom.is_slot_empty(SLOT_COCOSDC)) {
                flash_rom.status_flags |= FLASH_STATUS_SDC_MISSING;
            }
            ok = true;
            break;
        case FLASH_CMD_INTERNAL_ROM:
            reg_disk = 3;
            pending_mode_switch = MODE_INTERNAL_ROM;
            ok = true;
            break;
        default:
            break;
    }

    flash_result = ok ? FLASH_RESULT_OK : FLASH_RESULT_ERROR;
    pending_flash_cmd = 0;
}

bool BootMenu::consume_pending_mode_switch(EmulatorMode* out_mode) {
    if (pending_mode_switch == MODE_BOOT_MENU || !out_mode) {
        return false;
    }
    *out_mode = pending_mode_switch;
    pending_mode_switch = MODE_BOOT_MENU;
    return true;
}

void BootMenu::service_flash_command() {
    if (pending_flash_cmd != 0 && flash_result == FLASH_RESULT_BUSY) {
        run_flash_command(pending_flash_cmd);
    }
}

EmulatorMode BootMenu::calculate_mode() {
    if (reg_audio == 1) return MODE_ORCH90;
    if (reg_audio == 2) return MODE_SPEECH_SOUND;
    if (reg_video == 1) return MODE_WORDPAK2;

    if (reg_disk == 0) return MODE_COCOSDC;
    if (reg_disk == 1 || reg_comm == 2) return MODE_FUJINET;
    if (reg_disk == 3) return MODE_INTERNAL_ROM;

    if (reg_comm == 1) return MODE_RS232_PAK_LEGACY;
    if (reg_comm == 3) return MODE_WIMODEM;

    return MODE_BOOT_MENU;
}
