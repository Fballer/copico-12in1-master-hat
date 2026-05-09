#ifndef BOOT_MENU_H
#define BOOT_MENU_H

#include <Arduino.h>

class BootMenu {
public:
    BootMenu();
    void init();
    uint8_t read_rom(uint16_t address);
    
private:
    static const uint8_t boot_rom[8192];
};

#endif // BOOT_MENU_H
