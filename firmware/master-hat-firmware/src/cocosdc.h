#ifndef COCOSDC_H
#define COCOSDC_H

#include <Arduino.h>

class Cocosdc {
public:
    Cocosdc();
    
    // Core 0: High-level SD initialization and mounting
    void init();
    
    // Core 0: Background task for handling SD reads/writes
    void tick();
    
    // Core 1: Fast memory access for the shadowed $C000-$DFFF ROM
    uint8_t read_rom(uint16_t addr);
    
    // Core 1: Fast register access for $FF40-$FF4B
    uint8_t read_register(uint16_t addr);
    void write_register(uint16_t addr, uint8_t data);

private:
    // Memory
    uint8_t rom_buffer[8192];
    uint8_t sector_buffer[256];
    
    // Hardware Emulation State
    bool extended_mode_active;
    volatile uint8_t status_reg;      // $FF48 Read
    volatile uint32_t current_lsn;    // 24-bit Logical Sector Number
    volatile uint16_t buffer_index;   // For 256-byte transfers
    volatile uint8_t pending_command; // Triggers Core 0 to act
    volatile uint8_t active_write_command; // Stores the command during CoCo buffer filling
    
    // ESP32 Bridge Tracking
    String drive_paths[2];
    
    // Internal Helpers
    void parse_startup_cfg();
    void mount_drive(uint8_t drive_num, const String& filename);
};

#endif // COCOSDC_H
