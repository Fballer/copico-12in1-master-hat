#ifndef BOOT_MENU_H
#define BOOT_MENU_H

#include <Arduino.h>
#include "emulator_mode.h"

class BootMenu {
public:
    BootMenu();
    void init();
    uint8_t read_rom(uint16_t address);
    void update_sys_info(const char* wifi, const char* fw, const char* sd, const char* tz);
    void set_config(uint16_t address, uint8_t data);
    EmulatorMode calculate_mode();

private:
    char wifi_status[32];
    char fw_version[32];
    char sd_status[32];
    char tz_status[32];

    uint8_t reg_disk = 0;
    uint8_t reg_comm = 0;
    uint8_t reg_audio = 0;
    uint8_t reg_video = 0;
};

#endif // BOOT_MENU_H
