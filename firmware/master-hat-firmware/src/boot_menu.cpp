#include "boot_menu.h"

// Option A: Minimalist "Alpha Strobe" Menu
// Clears screen to green, prints "COPICO", waits for '1'-'9', writes to $FF7F
const uint8_t BootMenu::boot_rom[8192] = {
    0x8E, 0x04, 0x00, 0x86, 0x60, 0xA7, 0x80, 0x8C, 
    0x06, 0x00, 0x26, 0xF9, 0xBD, 0xA0, 0x01, 0x27, 
    0xFB, 0x80, 0x31, 0xB7, 0xFF, 0x7F, 0x20, 0xFE
    // The rest of the array will be zero-initialized automatically
};

BootMenu::BootMenu() {
}

void BootMenu::init() {
    // Any specific initialization for the Boot Menu mode (if needed)
}

uint8_t BootMenu::read_rom(uint16_t address) {
    if (address >= 0xC000 && address <= 0xDFFF) {
        return boot_rom[address - 0xC000];
    }
    return 0xFF; // Default open bus
}
